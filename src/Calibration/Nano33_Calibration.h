#ifndef NANO33_CALIBRATION_H
#define NANO33_CALIBRATION_H

#include <FS_Nano33BLE.h>
#include <Arduino_LSM9DS1.h>
#include <Fusion/Fusion.h>

// Constants
#define CALIBRATION_FILE MBED_FS_FILE_PREFIX "/calibration.dat"

// LED Pin definitions
#define PIN_LED     (13u)
#define LED_BUILTIN PIN_LED
#define RED        (22u)
#define GREEN      (23u)
#define BLUE       (24u)
#define LED_PWR    (25u)

// Structs and variables
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

// Extern variables
extern CalibrationData calibrationData;
extern FusionMatrix gyroscopeMisalignment;
extern FusionVector gyroscopeSensitivity;
extern FusionVector gyroscopeOffset;
extern FusionMatrix accelerometerMisalignment;
extern FusionVector accelerometerSensitivity;
extern FusionVector accelerometerOffset;
extern FusionMatrix softIronMatrix;
extern FusionVector hardIronOffset;

extern FileSystem_MBED *myFS;

// Function declarations
void initFS();
void clearCalibrationData();
void saveCalibrationData();
bool loadCalibrationData();
void calibrateGyroscope();
void calibrateAccelerometer();
void calibrateMagnetometer();
void runCalibrationSequence();

#endif // NANO33_CALIBRATION_H