//Nano33_Calibration.cpp

#include <FS_Nano33BLE.h>
#include <float.h>
#include <Arduino_LSM9DS1.h>
#include <Utils/Utils.h>
#include <Fusion/Fusion.h>
#include <BasicLinearAlgebra.h>

#define CALIBRATION_FILE MBED_FS_FILE_PREFIX "/calibration.dat"

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
            CaX *= -1.0;

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
// Collect Magnetometer Data
///////////////////////////////////////////////////////////////////
void collectMagnetometerData() {
    sampleCount = 0;
    while (sampleCount < 1000) {
        if (IMU.magneticFieldAvailable()) {
            IMU.readRawMagnet(magData[sampleCount][0],
                              magData[sampleCount][1],
                              magData[sampleCount][2]);
            sampleCount++;
            delay(10);
        }
    }
}

///////////////////////////////////////////////////////////////////
// Magnetometer Calibration - Improved
///////////////////////////////////////////////////////////////////
bool fitEllipsoid(float data[][3], int numSamples, FusionVector &offset, FusionMatrix &softIronMatrix) {
    using namespace BLA;

    BLA::Matrix<9, 9> A;
    BLA::Matrix<9> B;

    A.Fill(0.0);
    B.Fill(0.0);

    for (int i = 0; i < numSamples; i++) {
        float x = data[i][0], y = data[i][1], z = data[i][2];
        float d[9] = {x * x, y * y, z * z, 2 * x * y, 2 * x * z, 2 * y * z, 2 * x, 2 * y, 2 * z};
        for (int j = 0; j < 9; j++) {
            for (int k = 0; k < 9; k++) {
                A(j, k) += d[j] * d[k];
            }
            B(j) -= d[j];
        }
    }

    for (int i = 0; i < 9; i++) {
        float pivot = A(i, i);
        for (int j = 0; j < 9; j++) {
            A(i, j) /= pivot;
        }
        B(i) /= pivot;

        for (int k = 0; k < 9; k++) {
            if (k != i) {
                float factor = A(k, i);
                for (int j = 0; j < 9; j++) {
                    A(k, j) -= factor * A(i, j);
                }
                B(k) -= factor * B(i);
            }
        }
    }

    offset.axis.x = -B(6) / (2 * B(0));
    offset.axis.y = -B(7) / (2 * B(1));
    offset.axis.z = -B(8) / (2 * B(2));

    softIronMatrix = {
        1.0f / sqrt(B(0)), 0.0f, 0.0f,
        0.0f, 1.0f / sqrt(B(1)), 0.0f,
        0.0f, 0.0f, 1.0f / sqrt(B(2))
    };

    return true;
}

///////////////////////////////////////////////////////////////////
// Magnetometer Calibration
///////////////////////////////////////////////////////////////////
void calibrateMagnetometer() {
    logString("Calibrating magnetometer... Move the device in a figure-eight pattern.", true);

    collectMagnetometerData();

    FusionVector hardIronOffset;
    FusionMatrix softIronMatrix;
    if (fitEllipsoid(magData, sampleCount, hardIronOffset, softIronMatrix)) {
        calibrationData.hardIronOffset = hardIronOffset;
        calibrationData.softIronMatrix = softIronMatrix;

        logString("Magnetometer calibration complete.", true);
        logString("Hard Iron Offset: " + FusionVectorToString(hardIronOffset), true);
        logString("Soft Iron Matrix:", true);
        logString("XX: " + String(softIronMatrix.element.xx, 6) + ", YY: " +
                  String(softIronMatrix.element.yy, 6) + ", ZZ: " +
                  String(softIronMatrix.element.zz, 6), true);
    } else {
        logString("Ellipsoid fitting failed. Calibration aborted.", true);
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

    // GYRO STAGE BLUE
    setColorLedState("blue"); // Turn on Blue LED to indicate calibration
    calibrateGyroscope();

    // ACCEL STAGE GREEN
    setColorLedState("green");
    calibrateAccelerometer();

    // MAG STAGE LIGHT BLUE
    setColorLedState("cyan");
    calibrateMagnetometer();

    setColorLedState("off");
    logString("Calibration sequence completed.", true);
}
