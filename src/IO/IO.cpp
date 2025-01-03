/*
* IO functions for the Nano 33 BLE
*/

#include <Utils/Utils.h>
#include "IO.h"

Hat hat = {
    .Begin = (int16_t)0xAAAA, // Explicit cast to int16_t
    .Cpt = 0,
    .gyro = {0.0f, 0.0f, 0.0f},
    .acc = {0.0f, 0.0f, 0.0f},
    .End = (int16_t)0x5555  // Explicit cast to int16_t
};
bool IOInitDone = false;
bool BLEconnected = false;
const char* deviceServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214";
const char* deviceCharacteristicUuid = "19b10001-e8f2-537e-4f6c-d104768a1214";
BLEService opentrackService(deviceServiceUuid);
BLECharacteristic hatireCharacteristic(
    deviceCharacteristicUuid,
    BLERead | BLEWrite | BLENotify,
    sizeof(hat)
);

///////////////////////////////////////////////////////////////////
// Sends HAT structure to Hatire using current communication method
///////////////////////////////////////////////////////////////////
void sendAnglesToHatire() {
    if (BLEconnected) {
        hatireCharacteristic.writeValue((byte *)&hat, sizeof(hat));
    } else {
        Serial.write((byte *)&hat, sizeof(hat));
    }

    hat.Cpt++;
    if (hat.Cpt > 999) {
        hat.Cpt = 0;
    }
}

///////////////////////////////////////////////////////////////////
// Init IO
///////////////////////////////////////////////////////////////////
void initIO() {

    //Hatire Bytes
    hat.Begin = 0xAAAA;
    hat.Cpt = 0;
    hat.End = 0x5555;

    logString("Starting BLE initialization...", true);

    // Initialize BLE
    if (!BLE.begin()) {
        logString("[ERROR] BLE initialization failed! Is the BLE hardware functional?", true);
        // Halt execution
    } else {
        logString("[INFO] BLE initialization successful.", true);
    }
    delay(100);
    // Set device local name
    BLE.setLocalName("Nano 33 Head Tracker");
    logString("[INFO] Set local name to 'Nano 33 Head Tracker'.", true);
    delay(100);
    // Set advertised service
    BLE.setAdvertisedService(opentrackService);
    logString("[INFO] Advertised service linked to BLE peripheral.", true);
    delay(100);
    // Add characteristic
    opentrackService.addCharacteristic(hatireCharacteristic);
    logString("[INFO] Added characteristic to the service.", true);
    delay(100);
    // Add service to BLE peripheral
    BLE.addService(opentrackService);
    logString("[INFO] Service added to BLE peripheral.", true);
    delay(100);
    // Initialize characteristic with default data
    if (!hatireCharacteristic.setValue((byte *)&hat, sizeof(hat))) {
        logString("[ERROR] Failed to set initial value for characteristic!", true);
    } else {
        logString("[INFO] Initial value set for characteristic.", true);
    }
    delay(100);

    // Start advertising
    if (!BLE.advertise()) {
        logString("[ERROR] BLE advertising failed to start!", true);
    } else {
        logString("[INFO] BLE advertising started successfully.", true);
    }

    delay(100);
    if (!TestMode) {
        Serial.begin(115200);
    }

    // Indicate IO initialization complete
    setColorLedState("purple");
    IOInitDone = true;
    logString("[INFO] IO initialization complete.", true);
}