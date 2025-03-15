//Nano33_Calibration.cpp

#include "Nano33_Calibration.h"

#define CALIBRATION_FILE MBED_FS_FILE_PREFIX "/calibration.dat"

#define MIN_MAG_SAMPLES 300      // Keep minimum samples
#define MAX_MAG_SAMPLES 600      // Keep maximum samples
#define MIN_RADIUS_VARIATION 0.25f // Slightly more sensitive
#define MAG_SAMPLE_PERIOD_MS 20   // Back to original faster rate
#define GYRO_SAMPLES 500         // Reduced - just need basic offset
#define GYRO_STABILITY_THRESHOLD 0.02f  // For offset calculation
#define ACCEL_SAMPLES_PER_ORIENTATION 100  // Reduced from 200 for faster calibration
#define ACCEL_NUM_STABLE_READINGS 50    // Number of stable readings needed
#define ACCEL_GRAVITY_THRESHOLD 0.1f    // 0.1G threshold for gravity detection
#define ACCEL_STABILITY_THRESHOLD 0.05f  // 0.05G threshold for stability
#define GRAVITY_REFERENCE 1.0f         // 1G reference (not 9.81 m/s²)
#define CALIBRATION_TIMEOUT_MS 30000 // 30 second timeout
#define MOVING_AVERAGE_SAMPLES 10       // Number of samples for moving average
#define VARIANCE_CHECK_SAMPLES 20       // Samples to check for variance
#define MIN_SAMPLES_PER_SECTOR 10       // Even more lenient per sector

// Define calibration
FusionMatrix gyroscopeMisalignment = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
FusionVector gyroscopeSensitivity = {1.0f, 1.0f, 1.0f};
FusionVector gyroscopeOffset = {0.0f, 0.0f, 0.0f};
FusionMatrix accelerometerMisalignment = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
FusionVector accelerometerSensitivity = {1.0f, 1.0f, 1.0f};
FusionVector accelerometerOffset = {0.0f, 0.0f, 0.0f};
FusionMatrix softIronMatrix = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
FusionVector hardIronOffset = {0.0f, 0.0f, 0.0f};

CalibrationData calibrationData;

// Structure to track calibration progress
struct CalibrationProgress {
    int totalSteps;
    int currentStep;
    const char* currentStage;
    float percentComplete;
    
    void update(int step, const char* stage, float percent) {
        currentStep = step;
        currentStage = stage;
        percentComplete = percent;
        reportProgress();
    }
    
    void reportProgress() {
        logString("Calibration Progress: Stage ", false);
        logString(currentStep, false);
        logString("/", false);
        logString(totalSteps, false);
        logString(" - ", false);
        logString(currentStage, false);
        logString(" (", false);
        logString(percentComplete, false);
        logString("%)", true);
    }
};

// Structure to track magnetometer calibration stats
struct MagCalibrationStats {
    float minX = FLT_MAX, maxX = -FLT_MAX;
    float minY = FLT_MAX, maxY = -FLT_MAX;
    float minZ = FLT_MAX, maxZ = -FLT_MAX;
    
    // Track samples in different spatial sectors
    int sectorSamples[8] = {0}; // 8 sectors (octants) of 3D space
    float lastX = 0, lastY = 0, lastZ = 0; // Track last sample for movement detection
    bool firstSample = true;
    
    int getSector(float x, float y, float z) {
        // Normalize values relative to current min/max
        float nx = (x - minX) / (maxX - minX + 0.0001f);
        float ny = (y - minY) / (maxY - minY + 0.0001f);
        float nz = (z - minZ) / (maxZ - minZ + 0.0001f);
        
        int sector = 0;
        if (nx > 0.5f) sector |= 1;
        if (ny > 0.5f) sector |= 2;
        if (nz > 0.5f) sector |= 4;
        return sector;
    }
    
    bool isMoving(float x, float y, float z) {
        if (firstSample) {
            firstSample = false;
            lastX = x;
            lastY = y;
            lastZ = z;
            return false;
        }
        
        float dx = x - lastX;
        float dy = y - lastY;
        float dz = z - lastZ;
        
        lastX = x;
        lastY = y;
        lastZ = z;
        
        // Calculate movement magnitude - reduced threshold for more sensitivity
        float movement = sqrt(dx*dx + dy*dy + dz*dz);
        return movement > 0.01f; // More sensitive movement detection
    }
    
    void addSample(float x, float y, float z) {
        // Update min/max regardless of movement to track full range
        minX = min(minX, x);
        maxX = max(maxX, x);
        minY = min(minY, y);
        maxY = max(maxY, y);
        minZ = min(minZ, z);
        maxZ = max(maxZ, z);
        
        // Only increment sector counts if moving
        if (isMoving(x, y, z)) {
            int sector = getSector(x, y, z);
            sectorSamples[sector]++;
        }
    }
    
    bool hasGoodCoverage() {
        // Check basic range requirements
        float xRange = maxX - minX;
        float yRange = maxY - minY;
        float zRange = maxZ - minZ;
        bool rangeOK = (xRange > MIN_RADIUS_VARIATION && 
                       yRange > MIN_RADIUS_VARIATION && 
                       zRange > MIN_RADIUS_VARIATION);
        
        // Check sector coverage - require at least 4 sectors with minimum samples
        int goodSectors = 0;
        for (int i = 0; i < 8; i++) {
            if (sectorSamples[i] >= MIN_SAMPLES_PER_SECTOR) {
                goodSectors++;
            }
        }
        
        return rangeOK && (goodSectors >= 4);
    }
    
    float getCoverage() {
        // Count sectors that meet minimum samples
        int goodSectors = 0;
        int totalSamples = 0;
        
        for (int i = 0; i < 8; i++) {
            totalSamples += sectorSamples[i];
            if (sectorSamples[i] >= MIN_SAMPLES_PER_SECTOR) {
                goodSectors++;
            }
        }
        
        // Calculate range coverage
        float xRange = maxX - minX;
        float yRange = maxY - minY;
        float zRange = maxZ - minZ;
        
        // More gradual range coverage calculation
        float rangeCoverage = (
            (xRange / MIN_RADIUS_VARIATION) + 
            (yRange / MIN_RADIUS_VARIATION) + 
            (zRange / MIN_RADIUS_VARIATION)
        ) / 3.0f;
        
        // Combine sector and range coverage with early feedback
        float sectorCoverage = goodSectors / 4.0f; // Need 4 sectors for good coverage
        float totalCoverage = (rangeCoverage + sectorCoverage) / 2.0f;
        
        // Start showing cyan earlier but still require full coverage for completion
        return min(1.0f, totalCoverage);
    }
};

CalibrationProgress progress = {3, 0, "", 0.0f}; // 3 stages: gyro, accel, mag

// Sensor variables
float CgX = 0, CgY = 0, CgZ = 0;
float CaX = 0, CaY = 0, CaZ = 0;
float CmX = 0, CmY = 0, CmZ = 0;
float Cdeltat;

// FileSystem object
FileSystem_MBED *myFS;

// Buffer for magnetometer data
float magData[1000][3];
int sampleCount = 0;

String FusionVectorToString(const FusionVector &vector) {
    String result = "(";
    result += String(vector.axis.x, 6) + ", ";
    result += String(vector.axis.y, 6) + ", ";
    result += String(vector.axis.z, 6) + ")";
    return result;
}

///////////////////////////////////////////////////////////////////
// Init filesystem
///////////////////////////////////////////////////////////////////
void initFS() {
    myFS = new FileSystem_MBED();
    while (!myFS);
}

///////////////////////////////////////////////////////////////////
// Clear calibration data from flash
///////////////////////////////////////////////////////////////////
void clearCalibrationData() {
    // Check if the file system is initialized
    if (!myFS) {
        logString("Error: File system is not initialized. Cannot clear calibration data.", true);
        return;
    }

    logString("Invalidating calibration data...", true);

    // Attempt to open the calibration file for reading and writing
    FILE *file = fopen(CALIBRATION_FILE, "r+"); // Open file in read/write mode
    if (file) {
        // Read the current calibration data
        CalibrationData currentData;
        fread((uint8_t *)&currentData, sizeof(CalibrationData), 1, file);

        // Set the valid flag to false
        currentData.valid = false;

        // Move the file pointer back to the beginning
        fseek(file, 0, SEEK_SET);

        // Write the updated data back to the file
        size_t written = fwrite((uint8_t *)&currentData, sizeof(CalibrationData), 1, file);
        fclose(file);

        // Verify the write operation
        if (written == 1) {
            logString("Calibration data invalidated successfully. Reboot to start fresh calibration.", true);
        } else {
            logString("Error: Failed to update calibration data.", true);
        }
    } else {
        logString("Error: Failed to open calibration file for updating.", true);
    }
}

///////////////////////////////////////////////////////////////////
// Save IMU calibration data to flash
///////////////////////////////////////////////////////////////////
void saveCalibrationData() {
    calibrationData.valid = true; // Mark the data as valid
    FILE *file = fopen(CALIBRATION_FILE, "w");
    if (file) {
        fwrite((uint8_t *)&calibrationData, sizeof(CalibrationData), 1, file);
        fclose(file);
        logString("Calibration data saved to flash.", true);
    } else {
        logString("Failed to open file for writing calibration data.", true);
    }
}

///////////////////////////////////////////////////////////////////
// Load IMU calibration data from flash
///////////////////////////////////////////////////////////////////
bool loadCalibrationData() {
    FILE *file = fopen(CALIBRATION_FILE, "r");
    if (file) {
        fread((uint8_t *)&calibrationData, sizeof(CalibrationData), 1, file);
        fclose(file);

        if (calibrationData.valid) {
            logString("Calibration data loaded from flash:", true);

            // Print Gyroscope Calibration
            logString("Gyro Offsets: ", false);
            logString(calibrationData.gyroscopeOffset.axis.x, false);
            logString(", ", false);
            logString(calibrationData.gyroscopeOffset.axis.y, false);
            logString(", ", false);
            logString(calibrationData.gyroscopeOffset.axis.z, true);

            // Not saved currently, using defaults
            logString("Gyro Misalignment Matrix:", true);
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    logString(calibrationData.gyroscopeMisalignment.array[row][col], false);
                    logString(col < 2 ? ", " : "\n", false);
    
                }
            }

            logString("Gyro Sensitivity: ", false);
            logString(calibrationData.gyroscopeSensitivity.axis.x, false);
            logString(", ", false);
            logString(calibrationData.gyroscopeSensitivity.axis.y, false);
            logString(", ", false);
            logString(calibrationData.gyroscopeSensitivity.axis.z, true);

            // Print Accelerometer Calibration
            logString("Accel Offsets: ", false);
            logString(calibrationData.accelerometerOffset.axis.x, false);
            logString(", ", false);
            logString(calibrationData.accelerometerOffset.axis.y, false);
            logString(", ", false);
            logString(calibrationData.accelerometerOffset.axis.z, true);

            // Not currently saved, using defaults
            Serial.println("Accel Misalignment Matrix:");
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    logString(calibrationData.accelerometerMisalignment.array[row][col], false);
                    logString(col < 2 ? ", " : "\n", false);
                }
            }

            // Print Accelerometer Calibration
            logString("Accel Sensitivity: ", false);
            logString(calibrationData.accelerometerSensitivity.axis.x, false);
            logString(", ", false);
            logString(calibrationData.accelerometerSensitivity.axis.y, false);
            logString(", ", false);
            logString(calibrationData.accelerometerSensitivity.axis.z, true);

            // Print Magnetometer Calibration (not currently saved, using defaults)
            logString("Mag Hard Iron Offsets: ", false);
            logString(calibrationData.hardIronOffset.axis.x, false);
            logString(", ", false);
            logString(calibrationData.hardIronOffset.axis.y, false);
            logString(", ", false);
            logString(calibrationData.hardIronOffset.axis.z, true);

            logString("Soft Iron Matrix:", true);
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    logString(calibrationData.softIronMatrix.array[row][col], false);
                    logString(col < 2 ? ", " : "\n", false);
                }
            }

            return true;
        } else {
            logString("Calibration data is invalid. Starting calibration sequence.", true);
            return false;
        }
    } else {
        logString("No calibration data found. Starting calibration sequence.", true);
        return false;
    }
}

///////////////////////////////////////////////////////////////////
// Magnetometer Calibration - Main focus, more robust
///////////////////////////////////////////////////////////////////
void calibrateMagnetometer() {
    bool calibrationSuccess = false;
    FusionVector minValues = {FLT_MAX, FLT_MAX, FLT_MAX};
    FusionVector maxValues = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    MagCalibrationStats stats;
    int samples = 0;
    unsigned long startTime = millis();
    bool ledState = true;
    unsigned long lastLedUpdate = 0;
    
    // Arrays to store samples for correlation analysis
    float magX[MAX_MAG_SAMPLES] = {0};
    float magY[MAX_MAG_SAMPLES] = {0};
    float magZ[MAX_MAG_SAMPLES] = {0};
    
    while (samples < MAX_MAG_SAMPLES && (millis() - startTime) < CALIBRATION_TIMEOUT_MS) {
        if (imuHandler.magnetAvailable()) {
            float mx, my, mz;
            imuHandler.readRawMagnet(mx, my, mz);
            
            // Store samples for correlation analysis
            if (samples < MAX_MAG_SAMPLES) {
                magX[samples] = mx;
                magY[samples] = my;
                magZ[samples] = mz;
            }
            
            // Update stats and increment samples
            stats.addSample(mx, my, mz);
            samples++;
            
            // Update LED based on coverage - more responsive feedback
            if (millis() - lastLedUpdate > 100) { // Faster LED updates
                float coverage = stats.getCoverage();
                if (coverage >= 0.8f) { // Show cyan earlier
                    setColorLedState("cyan"); // Solid cyan when getting close
                } else if (coverage >= 0.4f) { // Start transition phase
                    setColorLedState(ledState ? "cyan" : "purple");
                } else {
                    setColorLedState("purple"); // Solid purple when starting
                }
                ledState = !ledState;
                lastLedUpdate = millis();
            }
        }
        delay(MAG_SAMPLE_PERIOD_MS);
    }
    
    if (samples >= MIN_MAG_SAMPLES && stats.hasGoodCoverage()) {
        // Calculate hard iron offset (center of min/max)
        calibrationData.hardIronOffset = {
            (maxValues.axis.x + minValues.axis.x) / 2.0f,
            (maxValues.axis.y + minValues.axis.y) / 2.0f,
            (maxValues.axis.z + minValues.axis.z) / 2.0f
        };
        
        // Calculate correlations for soft iron matrix
        float corrXY = 0, corrXZ = 0, corrYZ = 0;
        float meanX = 0, meanY = 0, meanZ = 0;
        float varX = 0, varY = 0, varZ = 0;
        
        // Calculate means
        for (int i = 0; i < samples; i++) {
            meanX += magX[i];
            meanY += magY[i];
            meanZ += magZ[i];
        }
        meanX /= samples;
        meanY /= samples;
        meanZ /= samples;
        
        // Calculate variances and correlations
        for (int i = 0; i < samples; i++) {
            float dx = magX[i] - meanX;
            float dy = magY[i] - meanY;
            float dz = magZ[i] - meanZ;
            
            varX += dx * dx;
            varY += dy * dy;
            varZ += dz * dz;
            
            corrXY += dx * dy;
            corrXZ += dx * dz;
            corrYZ += dy * dz;
        }
        
        varX /= samples;
        varY /= samples;
        varZ /= samples;
        corrXY /= samples;
        corrXZ /= samples;
        corrYZ /= samples;
        
        // Normalize correlations
        corrXY /= sqrt(varX * varY);
        corrXZ /= sqrt(varX * varZ);
        corrYZ /= sqrt(varY * varZ);
        
        // Create soft iron matrix with cross-terms
        float maxRange = sqrt(max(max(varX, varY), varZ));
        calibrationData.softIronMatrix = {
            maxRange / sqrt(varX), corrXY * 0.5f, corrXZ * 0.5f,
            corrXY * 0.5f, maxRange / sqrt(varY), corrYZ * 0.5f,
            corrXZ * 0.5f, corrYZ * 0.5f, maxRange / sqrt(varZ)
        };
        
        calibrationSuccess = true;
        logString("Magnetometer calibration successful!", true);
        logString("Hard iron offsets: ", false);
        logString(calibrationData.hardIronOffset.axis.x, false); logString(", ", false);
        logString(calibrationData.hardIronOffset.axis.y, false); logString(", ", false);
        logString(calibrationData.hardIronOffset.axis.z, true);
    } else {
        calibrationSuccess = false;
        logString("Magnetometer calibration failed - insufficient coverage", true);
    }
    
    calibrationData.valid = calibrationSuccess;
}

///////////////////////////////////////////////////////////////////
// Run Calibration Sequence - Simplified
///////////////////////////////////////////////////////////////////
void runCalibrationSequence() {
    logString("=== Starting Calibration Sequence ===", true);
    logString("This will calibrate gyro offset and magnetometer", true);
    logString("Accelerometer using factory calibration", true);
    
    // Start with white flash to indicate calibration start
    setColorLedState("white");
    delay(500);
    setColorLedState("off");
    delay(500);
    
    // Initialize matrices and set validity to false initially
    calibrationData.valid = false;  // Start with invalid state
    calibrationData.gyroscopeMisalignment = FUSION_IDENTITY_MATRIX;
    calibrationData.accelerometerMisalignment = FUSION_IDENTITY_MATRIX;
    calibrationData.softIronMatrix = FUSION_IDENTITY_MATRIX;
    
    // Use factory accel calibration
    calibrationData.accelerometerOffset = {0.0f, 0.0f, 0.0f};
    calibrationData.accelerometerSensitivity = {1.0f, 1.0f, 1.0f};
    logString("Using factory accelerometer calibration", true);
    
    // Quick gyro offset calibration - BLUE blinking
    progress.update(1, "Keep device still for gyro offset", 0);
    logString("=== Quick Gyro Offset Calibration ===", true);
    logString("Keep device still on flat surface", true);
    
    // Three quick blue flashes to indicate start of gyro cal
    for (int i = 0; i < 3; i++) {
        setColorLedState("blue");
        delay(200);
        setColorLedState("off");
        delay(200);
    }
    
    float sumX = 0, sumY = 0, sumZ = 0;
    int samples = 0;
    unsigned long startTime = millis();
    
    // Solid BLUE during gyro calibration
    setColorLedState("blue");
    
    while (samples < GYRO_SAMPLES && (millis() - startTime) < CALIBRATION_TIMEOUT_MS) {
        if (imuHandler.gyroAvailable()) {
            float gx, gy, gz;
            imuHandler.readRawGyro(gx, gy, gz);
            sumX += gx;
            sumY += gy;
            sumZ += gz;
            samples++;
        }
        delay(2);
    }
    
    bool gyroSuccess = (samples >= GYRO_SAMPLES);
    if (gyroSuccess) {
        calibrationData.gyroscopeOffset = {
            sumX / samples,
            sumY / samples,
            sumZ / samples
        };
        calibrationData.gyroscopeSensitivity = {1.0f, 1.0f, 1.0f};
        logString("Gyro offset calibration complete", true);
        
        // Flash green three times to indicate success
        for (int i = 0; i < 3; i++) {
            setColorLedState("green");
            delay(200);
            setColorLedState("off");
            delay(200);
        }
    } else {
        logString("Gyro calibration failed - timeout", true);
        // Flash red three times to indicate failure
        for (int i = 0; i < 3; i++) {
            setColorLedState("red");
            delay(200);
            setColorLedState("off");
            delay(200);
        }
    }
    
    delay(1000); // Pause before next stage
    
    // Only proceed with mag cal if gyro was successful
    if (gyroSuccess) {
        // Magnetometer calibration - CYAN/PURPLE pattern
        progress.update(2, "Magnetometer Calibration - Draw figure-8 patterns", 0);
        logString("=== Magnetometer Calibration ===", true);
        logString("1. Hold device level", true);
        logString("2. Draw figure-8 patterns in the air", true);
        logString("3. Rotate device to cover all orientations", true);
        
        // Three cyan flashes to indicate start of mag cal
        for (int i = 0; i < 3; i++) {
            setColorLedState("cyan");
            delay(200);
            setColorLedState("off");
            delay(200);
        }
        
        calibrateMagnetometer();
    }
    
    // Final status indication - make it very distinct from boot sequence
    if (calibrationData.valid) {
        // Success pattern: green-white-green-white-green (faster)
        for (int i = 0; i < 3; i++) {
            setColorLedState("green");
            delay(200);
            setColorLedState("white");
            delay(200);
        }
        setColorLedState("green");
        delay(500);
        
        // Save calibration data
        saveCalibrationData();
    } else {
        // Failure pattern: red-purple-red-purple-red (faster)
        for (int i = 0; i < 3; i++) {
            setColorLedState("red");
            delay(200);
            setColorLedState("purple");
            delay(200);
        }
        setColorLedState("red");
        delay(500);
    }
    
    setColorLedState("off");
    logString("=== Calibration Sequence Completed ===", true);
    delay(1000); // Give time to see the final state before any potential reboot
}