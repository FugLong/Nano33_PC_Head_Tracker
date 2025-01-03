#ifndef IO_H
#define IO_H

#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>

// Hatire Data Structure
struct Hat {
    int16_t Begin;        // Start marker
    uint16_t Cpt;         // Counter
    float gyro[3];        // Gyroscope data (Pitch, Roll, Yaw)
    float acc[3];         // Accelerometer data
    int16_t End;          // End marker
};

extern Hat hat;
extern bool IOInitDone;
extern bool BLEconnected;
extern const char* deviceServiceUuid;
extern const char* deviceCharacteristicUuid;
extern BLEService opentrackService;
extern BLECharacteristic hatireCharacteristic;

void sendAnglesToHatire();
void initIO();

#endif