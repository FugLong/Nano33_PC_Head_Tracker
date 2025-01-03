//Nano33_Calibration.cpp

#include <FS_Nano33BLE.h>
#include <float.h>
#include <Arduino_LSM9DS1.h>
#include <Utils/Utils.h>
#include <Fusion/Fusion.h>
#include <BasicLinearAlgebra.h>

#define CALIBRATION_FILE MBED_FS_FILE_PREFIX "/calibration.dat"

// Define calibration (replace with actual calibration data if available)
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

// Sensor variables
float CgX = 0, CgY = 0, CgZ = 0;
float CaX = 0, CaY = 0, CaZ = 0;
float CmX = 0, CmY = 0, CmZ = 0;
float Cdeltat;

// FileSystem object
FileSystem_MBED *myFS;

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
        Serial.println("Error: File system is not initialized. Cannot clear calibration data.");
        return;
    }

    Serial.println("Invalidating calibration data...");

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
            Serial.println("Calibration data invalidated successfully. Reboot to start fresh calibration.");
        } else {
            Serial.println("Error: Failed to update calibration data.");
        }
    } else {
        Serial.println("Error: Failed to open calibration file for updating.");
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
        Serial.println("Calibration data saved to flash.");
    } else {
        Serial.println("Failed to open file for writing calibration data.");
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
            Serial.println("Calibration data loaded from flash:");

            // Print Gyroscope Calibration
            Serial.print("Gyro Offsets: ");
            Serial.print(calibrationData.gyroscopeOffset.axis.x);
            Serial.print(", ");
            Serial.print(calibrationData.gyroscopeOffset.axis.y);
            Serial.print(", ");
            Serial.println(calibrationData.gyroscopeOffset.axis.z);

            Serial.println("Gyro Misalignment Matrix:");
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    Serial.print(calibrationData.gyroscopeMisalignment.array[row][col], 6);
                    Serial.print(col < 2 ? ", " : "\n");
                }
            }

            Serial.print("Gyro Sensitivity: ");
            Serial.print(calibrationData.gyroscopeSensitivity.axis.x);
            Serial.print(", ");
            Serial.print(calibrationData.gyroscopeSensitivity.axis.y);
            Serial.print(", ");
            Serial.println(calibrationData.gyroscopeSensitivity.axis.z);

            // Print Accelerometer Calibration
            Serial.print("Accel Offsets: ");
            Serial.print(calibrationData.accelerometerOffset.axis.x);
            Serial.print(", ");
            Serial.print(calibrationData.accelerometerOffset.axis.y);
            Serial.print(", ");
            Serial.println(calibrationData.accelerometerOffset.axis.z);

            Serial.println("Accel Misalignment Matrix:");
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    Serial.print(calibrationData.accelerometerMisalignment.array[row][col], 6);
                    Serial.print(col < 2 ? ", " : "\n");
                }
            }

            Serial.print("Accel Sensitivity: ");
            Serial.print(calibrationData.accelerometerSensitivity.axis.x);
            Serial.print(", ");
            Serial.print(calibrationData.accelerometerSensitivity.axis.y);
            Serial.print(", ");
            Serial.println(calibrationData.accelerometerSensitivity.axis.z);

            // Print Magnetometer Calibration
            Serial.print("Mag Hard Iron Offsets: ");
            Serial.print(calibrationData.hardIronOffset.axis.x);
            Serial.print(", ");
            Serial.print(calibrationData.hardIronOffset.axis.y);
            Serial.print(", ");
            Serial.println(calibrationData.hardIronOffset.axis.z);

            Serial.println("Soft Iron Matrix:");
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    Serial.print(calibrationData.softIronMatrix.array[row][col], 6);
                    Serial.print(col < 2 ? ", " : "\n");
                }
            }

            return true;
        } else {
            Serial.println("Calibration data is invalid. Starting calibration sequence.");
            return false;
        }
    } else {
        Serial.println("No calibration data found. Starting calibration sequence.");
        return false;
    }
}

///////////////////////////////////////////////////////////////////
// Gyro Calibration
///////////////////////////////////////////////////////////////////
void calibrateGyroscope() {
    Serial.println("Calibrating gyroscope... Ensure the device is stationary.");

    float gXSum = 0.0f, gYSum = 0.0f, gZSum = 0.0f;
    const int totalSamples = 500;

    for (int i = 0; i < totalSamples; i++) {
        if (IMU.gyroAvailable()) {
            IMU.readRawGyro(CgX, CgY, CgZ);
            gXSum += CgX;
            gYSum += CgY;
            gZSum += CgZ;
        }
        delay(10);
    }

    calibrationData.gyroscopeOffset = { gXSum / totalSamples, gYSum / totalSamples, gZSum / totalSamples };
    calibrationData.gyroscopeSensitivity = { 1.0f, 1.0f, 1.0f }; // Default sensitivity
    Serial.println("Gyroscope calibration complete.");
    Serial.print("Offsets: ");
    Serial.println(FusionVectorToString(calibrationData.gyroscopeOffset));
}

///////////////////////////////////////////////////////////////////
// Accelerometer Calibration
///////////////////////////////////////////////////////////////////
void calibrateAccelerometer() {
    Serial.println("Calibrating accelerometer... Rotate the device freely.");

    float aMin[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float aMax[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    for (int i = 0; i < 2000; i++) {
        if (IMU.accelAvailable()) {
            IMU.readRawAccel(CaX, CaY, CaZ);

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

    Serial.println("Accelerometer calibration complete.");
    Serial.print("Sensitivity: ");
    Serial.println(FusionVectorToString(calibrationData.accelerometerSensitivity));
}

///////////////////////////////////////////////////////////////////
// Magnetometer Calibration
///////////////////////////////////////////////////////////////////
void calibrateMagnetometer() {
    Serial.println("Calibrating magnetometer... Move the device in a figure-eight pattern.");

    const int maxSamples = 1000;       // Maximum samples to collect
    const int minSamples = 50;         // Minimum samples before checking range
    const float minRangeThreshold = 30.0f;  // Minimum acceptable range per axis
    const unsigned long noImprovementThreshold = 5000; // 5 seconds with no range improvement

    float magData[maxSamples][3];
    int collectedSamples = 0;

    // Initialize min and max values for range checking
    float mMin[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float mMax[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    // Track the last improvement in range
    unsigned long lastImprovementTime = millis();

    while (true) {
        if (IMU.magneticFieldAvailable()) {
            IMU.readRawMagnet(CmX, CmY, CmZ);

            // Store data if space is available
            if (collectedSamples < maxSamples) {
                magData[collectedSamples][0] = CmX;
                magData[collectedSamples][1] = CmY;
                magData[collectedSamples][2] = CmZ;
                collectedSamples++;
            }

            // Update min and max values and check for improvement
            bool improved = false;
            for (int i = 0; i < 3; i++) {
                if (CmX < mMin[i]) {
                    mMin[i] = CmX;
                    improved = true;
                }
                if (CmX > mMax[i]) {
                    mMax[i] = CmX;
                    improved = true;
                }
            }

            // Check for improvement
            if (improved) {
                lastImprovementTime = millis();
                Serial.print("Improved range detected. Current range (X, Y, Z): ");
                Serial.print(mMax[0] - mMin[0]);
                Serial.print(", ");
                Serial.print(mMax[1] - mMin[1]);
                Serial.print(", ");
                Serial.println(mMax[2] - mMin[2]);
            }

            // Feedback at intervals
            if (collectedSamples % 50 == 0) {
                Serial.print("Samples collected: ");
                Serial.println(collectedSamples);
                Serial.print("Current range (X, Y, Z): ");
                Serial.print(mMax[0] - mMin[0]);
                Serial.print(", ");
                Serial.print(mMax[1] - mMin[1]);
                Serial.print(", ");
                Serial.println(mMax[2] - mMin[2]);
                Serial.println("Keep moving the device in a figure-eight pattern.");
            }
        }

        // Ensure minimum samples are collected before evaluating range
        if (collectedSamples >= minSamples &&
            (mMax[0] - mMin[0] > minRangeThreshold) &&
            (mMax[1] - mMin[1] > minRangeThreshold) &&
            (mMax[2] - mMin[2] > minRangeThreshold)) {
            Serial.println("Sufficient range detected. Finalizing calibration...");
            break;
        }

        // Stop if no improvement is seen for a while
        if (millis() - lastImprovementTime > noImprovementThreshold && collectedSamples >= minSamples) {
            Serial.println("No significant improvement detected. Finalizing calibration...");
            break;
        }

        // End if maximum samples are collected
        if (collectedSamples >= maxSamples) {
            Serial.println("Maximum samples reached. Finalizing calibration...");
            break;
        }

        delay(50);  // Allow sensor to stabilize
    }

    // Calculate hard iron offsets
    calibrationData.hardIronOffset = {
        (mMax[0] + mMin[0]) / 2.0f,
        (mMax[1] + mMin[1]) / 2.0f,
        (mMax[2] + mMin[2]) / 2.0f
    };

    // Perform soft iron correction
    float deltaX = (mMax[0] - mMin[0]) / 2.0f;
    float deltaY = (mMax[1] - mMin[1]) / 2.0f;
    float deltaZ = (mMax[2] - mMin[2]) / 2.0f;

    if (deltaX < minRangeThreshold || deltaY < minRangeThreshold || deltaZ < minRangeThreshold) {
        Serial.println("Warning: Insufficient range detected. Using identity matrix for soft iron calibration.");
        calibrationData.softIronMatrix = FUSION_IDENTITY_MATRIX;
    } else {
        calibrationData.softIronMatrix = {
            1.0f / deltaX, 0.0f, 0.0f,
            0.0f, 1.0f / deltaY, 0.0f,
            0.0f, 0.0f, 1.0f / deltaZ
        };
    }

    // Debugging output
    Serial.println("Magnetometer calibration complete.");
    Serial.print("Hard Iron Offset: ");
    Serial.println(FusionVectorToString(calibrationData.hardIronOffset));
    Serial.println("Soft Iron Matrix: ");
    Serial.print("XX: ");
    Serial.print(calibrationData.softIronMatrix.element.xx, 6);
    Serial.print(", YY: ");
    Serial.print(calibrationData.softIronMatrix.element.yy, 6);
    Serial.print(", ZZ: ");
    Serial.println(calibrationData.softIronMatrix.element.zz, 6);
}

///////////////////////////////////////////////////////////////////
// Run Calibration for Gyro, Accel, and Mag
///////////////////////////////////////////////////////////////////
void runCalibrationSequence() {
    Serial.println("Starting calibration sequence...");
    calibrationData.gyroscopeMisalignment = FUSION_IDENTITY_MATRIX;
    calibrationData.accelerometerMisalignment = FUSION_IDENTITY_MATRIX;
    calibrationData.softIronMatrix = FUSION_IDENTITY_MATRIX; // For magnetometer

    //GYRO STAGE BLUE
    setColorLedState("blue"); // Turn on blue LED to indicate calibration
    calibrateGyroscope();

    //ACCEL STAGE GREEN
    setColorLedState("green");
    calibrateAccelerometer();

    /*MAG STAGE LIGHT BLUE
    digitalWrite(BLUE, LOW);//Blue ON
    calibrateMagnetometer();*/

    setColorLedState("off");
    Serial.println("Calibration sequence completed.");
}
