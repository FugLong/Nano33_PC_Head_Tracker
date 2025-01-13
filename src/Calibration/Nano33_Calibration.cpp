//Nano33_Calibration.cpp

#include <FS_Nano33BLE.h>
#include <float.h>
#include <Arduino_LSM9DS1.h>
#include <Utils/Utils.h>
#include <Fusion/Fusion.h>
#include <BasicLinearAlgebra.h>

#define CALIBRATION_FILE MBED_FS_FILE_PREFIX "/calibration.dat"

#define MIN_MAG_SAMPLES 2500  // Minimum samples needed
#define MAX_MAG_SAMPLES 5000  // Maximum samples to collect
#define MIN_RADIUS_VARIATION 0.3f  // Minimum variation needed in each axis
#define MAG_SAMPLE_PERIOD_MS 25  // 40Hz = 25ms period

// Define calibration
FusionMatrix gyroscopeMisalignment = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
FusionVector gyroscopeSensitivity = {1.0f, 1.0f, 1.0f};
FusionVector gyroscopeOffset = {0.0f, 0.0f, 0.0f};
FusionMatrix accelerometerMisalignment = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
FusionVector accelerometerSensitivity = {1.0f, 1.0f, 1.0f};
FusionVector accelerometerOffset = {0.0f, 0.0f, 0.0f};
FusionMatrix softIronMatrix = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
FusionVector hardIronOffset = {0.0f, 0.0f, 0.0f};

struct CalibrationData {
    bool valid;
    FusionMatrix gyroscopeMisalignment;
    FusionVector gyroscopeSensitivity;
    FusionVector gyroscopeOffset;
    FusionMatrix accelerometerMisalignment;
    FusionVector accelerometerSensitivity;
    FusionVector accelerometerOffset;
    FusionMatrix softIronMatrix;
    FusionVector hardIronOffset;
};
CalibrationData calibrationData;

// Structure to track calibration progress
struct MagCalibrationStats {
    float minX = FLT_MAX, maxX = -FLT_MAX;
    float minY = FLT_MAX, maxY = -FLT_MAX;
    float minZ = FLT_MAX, maxZ = -FLT_MAX;
    bool isComplete() {
        float xRange = maxX - minX;
        float yRange = maxY - minY;
        float zRange = maxZ - minZ;
        
        // Check if we have sufficient variation in all axes
        return (xRange > MIN_RADIUS_VARIATION && 
                yRange > MIN_RADIUS_VARIATION && 
                zRange > MIN_RADIUS_VARIATION);
    }
    void printStats() {
        logString("Current ranges:", true);
        logString("X range: ", false);
        logString(maxX - minX, true);
        logString("Y range: ", false);
        logString(maxY - minY, true);
        logString("Z range: ", false);
        logString(maxZ - minZ, true);
    }
};

// Sensor variables
float CgX = 0, CgY = 0, CgZ = 0;
float CaX = 0, CaY = 0, CaZ = 0;
float CmX = 0, CmY = 0, CmZ = 0;
float Cdeltat;

// FileSystem object
FileSystem_MBED *myFS;

// Buffer for magnetometer data
float magData[3000][3];
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
// Gyro Calibration
///////////////////////////////////////////////////////////////////
void calibrateGyroscope() {
    logString("Calibrating gyroscope... Ensure the device is stationary.", true);

    float gXSum = 0.0f, gYSum = 0.0f, gZSum = 0.0f;
    const int totalSamples = 500;

    for (int i = 0; i < totalSamples; i++) {
        if (IMU.gyroAvailable()) {
            IMU.readRawGyro(CgX, CgY, CgZ);
            CgX *= -1.0; //Invert for goofy imu
            gXSum += CgX;
            gYSum += CgY;
            gZSum += CgZ;
        }
        delay(10);
    }

    calibrationData.gyroscopeOffset = { gXSum / totalSamples, gYSum / totalSamples, gZSum / totalSamples };
    calibrationData.gyroscopeSensitivity = { 1.0f, 1.0f, 1.0f }; // Default sensitivity
    logString("Gyroscope calibration complete.", true);
    logString("Offsets: ", false);
    logString(FusionVectorToString(calibrationData.gyroscopeOffset), true);
}

///////////////////////////////////////////////////////////////////
// Accelerometer Calibration
///////////////////////////////////////////////////////////////////
void calibrateAccelerometer() {
    logString("Calibrating accelerometer... Rotate the device freely.", true);

    float aMin[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float aMax[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = 0; i < 2000; i++) {
        if (IMU.accelAvailable()) {
            IMU.readRawAccel(CaX, CaY, CaZ);
            CaX *= -1.0; //Invert for goofy imu

            aMin[0] = min(aMin[0], CaX);
            aMax[0] = max(aMax[0], CaX);
            aMin[1] = min(aMin[1], CaY);
            aMax[1] = max(aMax[1], CaY);
            aMin[2] = min(aMin[2], CaZ);
            aMax[2] = max(aMax[2], CaZ);
        }
        delay(10);
    }

    calibrationData.accelerometerSensitivity = { 
        2.0f / (aMax[0] - aMin[0]), 
        2.0f / (aMax[1] - aMin[1]), 
        2.0f / (aMax[2] - aMin[2]) 
    };
    calibrationData.accelerometerMisalignment = FUSION_IDENTITY_MATRIX; // Default

    logString("Accelerometer calibration complete.", true);
    logString("Sensitivity: ", false);
    logString(FusionVectorToString(calibrationData.accelerometerSensitivity), true);
}

///////////////////////////////////////////////////////////////////
// Magnetometer Data Collection
///////////////////////////////////////////////////////////////////
void collectMagnetometerData() {
    MagCalibrationStats stats;
    sampleCount = 0;
    unsigned long lastSampleTime = 0;
    unsigned long lastLedUpdate = 0;
    unsigned long lastStatusUpdate = 0;
    bool ledState = false;
    
    logString("Begin magnetometer calibration...", true);
    logString("Move device in figure-8 pattern until LED stays solid", true);
    
    while (sampleCount < MAX_MAG_SAMPLES) {
        unsigned long currentTime = millis();
        
        // Check for array bounds
        if (sampleCount >= MAX_MAG_SAMPLES) {
            logString("ERROR: Sample count exceeded maximum!", true);
            break;
        }
        
        // Update LED every 100ms
        if (currentTime - lastLedUpdate >= 100) {
            ledState = !ledState;
            setColorLedState(ledState ? "cyan" : "off");
            lastLedUpdate = currentTime;
        }
        
        // Print status every second
        if (currentTime - lastStatusUpdate >= 1000) {
            logString("Samples collected: ", false);
            logString(sampleCount, true);
            stats.printStats();
            lastStatusUpdate = currentTime;
        }
        
        // Collect samples at 40Hz
        if (currentTime - lastSampleTime >= MAG_SAMPLE_PERIOD_MS) {
            if (IMU.magneticFieldAvailable()) {
                float x, y, z;
                IMU.readRawMagnet(x, y, z);
                
                // Update min/max values
                stats.minX = min(stats.minX, x);
                stats.maxX = max(stats.maxX, x);
                stats.minY = min(stats.minY, y);
                stats.maxY = max(stats.maxY, y);
                stats.minZ = min(stats.minZ, z);
                stats.maxZ = max(stats.maxZ, z);
                
                // Store the sample
                magData[sampleCount][0] = x;
                magData[sampleCount][1] = y;
                magData[sampleCount][2] = z;
                sampleCount++;
                
                lastSampleTime = currentTime;
                
                // Check if we have enough good samples and sufficient variation
                if (sampleCount >= MIN_MAG_SAMPLES && stats.isComplete()) {
                    setColorLedState("cyan");  // Solid cyan indicates completion
                    delay(50);
                    logString("Sufficient magnetometer data collected.", true);
                    logString("Final sample count: ", false);
                    logString(sampleCount, true);
                    stats.printStats();
                    return;
                    return;
                }
            }
        }
        
        // Small delay to prevent tight-looping
        delay(1);
    }
    
    // If we exit the loop without completing calibration
    logString("WARNING: Calibration ended without completing!", true);
    logString("Samples collected: ", false);
    logString(sampleCount, true);
    stats.printStats();
}

///////////////////////////////////////////////////////////////////
// Fit ellipsoid math
///////////////////////////////////////////////////////////////////
bool fitEllipsoid(float data[][3], int numSamples, FusionVector &offset, FusionMatrix &softIronMatrix) {
    logString("Starting ellipsoid fitting with ", false);
    logString(numSamples, false);
    logString(" samples...", true);
    
    using namespace BLA;

    // Pre-allocate matrices
    BLA::Matrix<9, 9> A;
    BLA::Matrix<9> B;
    
    logString("Initializing matrices...", true);
    A.Fill(0.0);
    B.Fill(0.0);

    // Build the design matrix A and target vector B
    logString("Building matrices...", true);
    for (int i = 0; i < numSamples; i++) {
        if (i % 500 == 0) {
            logString("Processing sample ", false);
            logString(i, true);
        }
        
        float x = data[i][0], y = data[i][1], z = data[i][2];
        float x2 = x * x, y2 = y * y, z2 = z * z;
        float xy = 2 * x * y, xz = 2 * x * z, yz = 2 * y * z;
        
        // Update A matrix
        float row[9] = {x2, y2, z2, xy, xz, yz, 2*x, 2*y, 2*z};
        for (int j = 0; j < 9; j++) {
            for (int k = 0; k < 9; k++) {
                A(j, k) += row[j] * row[k];
            }
            B(j) -= row[j];
        }
    }

    logString("Solving system...", true);
    
    // Gauss-Jordan elimination with pivoting
    for (int i = 0; i < 9; i++) {
        // Find pivot
        int pivotRow = i;
        float maxVal = abs(A(i, i));
        for (int j = i + 1; j < 9; j++) {
            if (abs(A(j, i)) > maxVal) {
                maxVal = abs(A(j, i));
                pivotRow = j;
            }
        }
        
        // Check for singularity
        if (maxVal < 1e-10) {
            logString("ERROR: Matrix is singular!", true);
            return false;
        }
        
        // Swap rows if needed
        if (pivotRow != i) {
            for (int j = 0; j < 9; j++) {
                float temp = A(i, j);
                A(i, j) = A(pivotRow, j);
                A(pivotRow, j) = temp;
            }
            float temp = B(i);
            B(i) = B(pivotRow);
            B(pivotRow) = temp;
        }
        
        // Normalize pivot row
        float pivot = A(i, i);
        for (int j = 0; j < 9; j++) {
            A(i, j) /= pivot;
        }
        B(i) /= pivot;
        
        // Eliminate column
        for (int j = 0; j < 9; j++) {
            if (j != i) {
                float factor = A(j, i);
                for (int k = 0; k < 9; k++) {
                    A(j, k) -= factor * A(i, k);
                }
                B(j) -= factor * B(i);
            }
        }
        
        if (i % 3 == 0) {
            logString("Solving... ", false);
            logString((i * 100) / 9, false);
            logString("%", true);
        }
    }

    logString("Computing calibration parameters...", true);
    
    // Extract the calibration parameters
    offset.axis.x = -B(6) / (2 * B(0));
    offset.axis.y = -B(7) / (2 * B(1));
    offset.axis.z = -B(8) / (2 * B(2));

    // Validate results
    if (isnan(offset.axis.x) || isnan(offset.axis.y) || isnan(offset.axis.z) ||
        abs(offset.axis.x) > 100 || abs(offset.axis.y) > 100 || abs(offset.axis.z) > 100) {
        logString("ERROR: Invalid calibration results!", true);
        return false;
    }

    // Compute soft iron matrix (simplified to diagonal)
    float sx = sqrt(abs(1.0f / B(0)));
    float sy = sqrt(abs(1.0f / B(1)));
    float sz = sqrt(abs(1.0f / B(2)));
    
    // Normalize scale factors
    float maxScale = max(max(sx, sy), sz);
    sx /= maxScale;
    sy /= maxScale;
    sz /= maxScale;
    
    softIronMatrix = {
        sx, 0.0f, 0.0f,
        0.0f, sy, 0.0f,
        0.0f, 0.0f, sz
    };

    logString("Ellipsoid fitting complete!", true);
    return true;
}

///////////////////////////////////////////////////////////////////
// Magnetometer Calibration
///////////////////////////////////////////////////////////////////
void calibrateMagnetometer() {
    logString("Starting magnetometer calibration...", true);
    logString("Move the device in all orientations.", true);
    
    collectMagnetometerData();
    
    // Only proceed with fitting if we have enough samples
    if (sampleCount >= MIN_MAG_SAMPLES) {
        FusionVector hardIronOffset;
        FusionMatrix softIronMatrix;
        
        if (fitEllipsoid(magData, sampleCount, hardIronOffset, softIronMatrix)) {
            calibrationData.hardIronOffset = hardIronOffset;
            calibrationData.softIronMatrix = softIronMatrix;
            
            logString("Magnetometer calibration successful.", true);
            logString("Hard Iron Offset: " + FusionVectorToString(hardIronOffset), true);
            logString("Soft Iron Matrix:", true);
            logString("XX: " + String(softIronMatrix.element.xx, 6) + 
                     ", YY: " + String(softIronMatrix.element.yy, 6) + 
                     ", ZZ: " + String(softIronMatrix.element.zz, 6), true);
        } else {
            logString("ERROR: Ellipsoid fitting failed.", true);
            // Use default values
            calibrationData.hardIronOffset = {0.0f, 0.0f, 0.0f};
            calibrationData.softIronMatrix = FUSION_IDENTITY_MATRIX;
        }
    } else {
        logString("ERROR: Not enough valid samples collected.", true);
        // Use default values
        calibrationData.hardIronOffset = {0.0f, 0.0f, 0.0f};
        calibrationData.softIronMatrix = FUSION_IDENTITY_MATRIX;
    }
}

///////////////////////////////////////////////////////////////////
// Run Calibration for Gyro, Accel, and Mag
///////////////////////////////////////////////////////////////////
void runCalibrationSequence() {
    logString("Starting calibration sequence...", true);
    calibrationData.gyroscopeMisalignment = FUSION_IDENTITY_MATRIX;
    calibrationData.accelerometerMisalignment = FUSION_IDENTITY_MATRIX;
    calibrationData.softIronMatrix = FUSION_IDENTITY_MATRIX; // For magnetometer

    // GYRO STAGE DARK BLUE
    setColorLedState("blue"); // Turn on Blue LED to indicate calibration
    calibrateGyroscope();

    // ACCEL STAGE GREEN
    setColorLedState("green");
    calibrateAccelerometer();

    // MAG STAGE CYAN
    setColorLedState("cyan");
    calibrateMagnetometer();

    setColorLedState("off");
    logString("Calibration sequence completed.", true);
}
