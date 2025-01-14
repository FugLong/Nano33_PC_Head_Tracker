#ifndef NANO33_CALIBRATION_H
#define NANO33_CALIBRATION_H

#include <FS_Nano33BLE.h>
#include <Arduino_LSM9DS1.h>
#include <Fusion/Fusion.h>
#include <utils/Utils.h>
#include <float.h>
#include <vector>
#include <numeric>
#include <deque>

// Constants
#define CALIBRATION_FILE MBED_FS_FILE_PREFIX "/calibration.dat"
#define MIN_MAG_SAMPLES 3000  // Minimum samples needed
#define MAX_MAG_SAMPLES 5000  // Maximum samples to collect
#define MIN_RADIUS_VARIATION 0.4f  // Minimum variation needed in each axis
#define MAG_SAMPLE_PERIOD_MS 25  // 40Hz = 25ms period
#define MIN_SAMPLES_PER_OCTANT 100  // Minimum samples needed in each octant
#define MAG_OUTLIER_THRESHOLD 3.0f  // Standard deviations for outlier detection

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

struct OctantStats {
    int sampleCount = 0;
    float sumX = 0, sumY = 0, sumZ = 0;
    float sumX2 = 0, sumY2 = 0, sumZ2 = 0;
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
bool fitSphere(const std::vector<FusionVector>& data, FusionVector& offset, FusionMatrix& sensitivity);
bool fitEllipsoid(float data[][3], int numSamples, FusionVector &offset, FusionMatrix &softIronMatrix);
void collectMagnetometerData();

#endif // NANO33_CALIBRATION_H