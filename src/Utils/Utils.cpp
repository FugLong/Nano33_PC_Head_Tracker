/*
* Utilities for the Nano 33 BLE
*/

#include <Arduino_LSM9DS1.h>
#include "Utils.h"

//-----------------------Battery Utils----------------------------

// Voltage Divider Resistor Values (Default: R1 = 150k, R2 = 300k)
const float R1Value = 150000.0; // 150k ohm
const float R2Value = 300000.0; // 300k ohm

// Battery monitoring
const float ADC_MAX_VOLTAGE = 3.3; // Max voltage for the ADC
const float LOW_BATTERY_THRESHOLD = 3.2; // Minimum safe battery voltage
const float MAX_BATTERY_VOLTAGE = 4.2; //Maximum voltage of input battery when fully charged

const bool TestMode = false;

///////////////////////////////////////////////////////////////////
// Update LEDs based on battery voltage
///////////////////////////////////////////////////////////////////
void updateBatteryLEDs(float batteryVoltage) {
    if (batteryVoltage <= 3.3) {
        // Flash red LED for low battery warning
        for (int i = 0; i < 3; i++) {
            digitalWrite(RED, LOW);
            delay(250);
            digitalWrite(RED, HIGH);
            delay(250);
        }
    } else {
        float range = MAX_BATTERY_VOLTAGE - LOW_BATTERY_THRESHOLD;
        float normalized = (batteryVoltage - LOW_BATTERY_THRESHOLD) / range;

        if (normalized > 0.66) {
            // Green for high battery level
            digitalWrite(GREEN, LOW);
            digitalWrite(BLUE, HIGH);
            digitalWrite(RED, HIGH);
        } else if (normalized > 0.33) {
            // Yellow for medium battery level
            digitalWrite(GREEN, LOW);
            digitalWrite(BLUE, LOW);
            digitalWrite(RED, HIGH);
        } else {
            // Red for low battery level
            digitalWrite(GREEN, HIGH);
            digitalWrite(BLUE, HIGH);
            digitalWrite(RED, LOW);
        }
    }
}

///////////////////////////////////////////////////////////////////
// Read battery voltage and scale it back up
///////////////////////////////////////////////////////////////////
float readBatteryVoltage() {
    int adcValue = analogRead(BATTERY_PIN);
    float voltageAtPin = (adcValue * ADC_MAX_VOLTAGE) / ((1 << ADC_RESOLUTION) - 1); // Voltage at A7
    float batteryVoltage = voltageAtPin * ((R1Value + R2Value) / R2Value); // Scale up

    return batteryVoltage;
}

///////////////////////////////////////////////////////////////////
// Check if battery monitoring circuit is present
///////////////////////////////////////////////////////////////////
bool isBatteryMonitoringAvailable() {
    float voltage = analogRead(BATTERY_PIN) * (ADC_MAX_VOLTAGE / ((1 << ADC_RESOLUTION) - 1));
    return voltage > 0.1; // A small threshold to detect if voltage is present
}

///////////////////////////////////////////////////////////////////
// Enter low power mode
///////////////////////////////////////////////////////////////////
void enterLowPowerMode() {
    logString("[WARN] Low battery detected! Entering low power mode...", true);
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(RED, HIGH);
    digitalWrite(GREEN, HIGH); //LED high Vs low is swapped on the color LEDs only, no clue why
    digitalWrite(BLUE, HIGH);
    digitalWrite(LED_PWR, LOW);
    // Enter the lowest power state available
}

///////////////////////////////////////////////////////////////////
// Battery voltage check
///////////////////////////////////////////////////////////////////
void checkBattery() {
    if (!isBatteryMonitoringAvailable()) {
        logString("[INFO] Battery monitoring circuit not detected. Skipping check.", true);
        return;
    }

    float batteryVoltage = readBatteryVoltage();
    logString("Battery Voltage: ", false);
    logString(batteryVoltage, true);

    if (batteryVoltage <= LOW_BATTERY_THRESHOLD) {
        enterLowPowerMode();
    }
}

//-----------------------General Utils----------------------------

///////////////////////////////////////////////////////////////////
// Setup pins for LED control and battery monitoring
///////////////////////////////////////////////////////////////////
void setupPins() {
    //Set LEDs to output
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(RED, OUTPUT);
    pinMode(BLUE, OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(LED_PWR, OUTPUT);

    //Set default LED states
    setPowerLedState(false);
    setDataLedState(false);
    setColorLedState("Red");

    // Set battery pin mode
    pinMode(BATTERY_PIN, INPUT);
}

///////////////////////////////////////////////////////////////////
// Log a string to serial if in test mode
///////////////////////////////////////////////////////////////////
void logString(String input, bool newLine) {
    if (TestMode){
        if (newLine){
            Serial.println(input);
        } else {
            Serial.print(input);
        }
    }
}

///////////////////////////////////////////////////////////////////
// Log a float to serial if in test mode
///////////////////////////////////////////////////////////////////
void logString(float input, bool newLine) {
    if (TestMode) {
        if (newLine) {
            Serial.println(String(input, 2)); // 2 specifies the number of decimal places
        } else {
            Serial.print(String(input, 2));
        }
    }
}

///////////////////////////////////////////////////////////////////
// Toggle data LED 
///////////////////////////////////////////////////////////////////
void setDataLedState(bool On) {
    if (On) {
        digitalWrite(LED_BUILTIN, HIGH);
    } else {
        digitalWrite(LED_BUILTIN, LOW);
    }
}

///////////////////////////////////////////////////////////////////
// Toggle power LED 
///////////////////////////////////////////////////////////////////
void setPowerLedState(bool On) {
    if (On) {
        digitalWrite(LED_PWR, HIGH);
    } else {
        digitalWrite(LED_PWR, LOW);
    }
}

///////////////////////////////////////////////////////////////////
// Set color of RGB LED (high and low inverted on this board for some reason)
///////////////////////////////////////////////////////////////////
void setColorLedState(String Color) {
    if (Color.equalsIgnoreCase("Off")) {
        digitalWrite(RED, HIGH);
        digitalWrite(GREEN, HIGH);
        digitalWrite(BLUE, HIGH);
    } else if (Color.equalsIgnoreCase("Red")) {
        digitalWrite(RED, LOW);
        digitalWrite(GREEN, HIGH);
        digitalWrite(BLUE, HIGH);
    } else if (Color.equalsIgnoreCase("Green")) {
        digitalWrite(RED, HIGH);
        digitalWrite(GREEN, LOW);
        digitalWrite(BLUE, HIGH);
    } else if (Color.equalsIgnoreCase("Blue")) {
        digitalWrite(RED, HIGH);
        digitalWrite(GREEN, HIGH);
        digitalWrite(BLUE, LOW);
    } else if (Color.equalsIgnoreCase("Orange")) {
        digitalWrite(RED, LOW);
        digitalWrite(GREEN, LOW);
        digitalWrite(BLUE, HIGH);
    } else if (Color.equalsIgnoreCase("Purple")) {
        digitalWrite(RED, LOW);
        digitalWrite(GREEN, HIGH);
        digitalWrite(BLUE, LOW);
    } else if (Color.equalsIgnoreCase("White")) {
        digitalWrite(RED, LOW);
        digitalWrite(GREEN, LOW);
        digitalWrite(BLUE, LOW);
    }
}

///////////////////////////////////////////////////////////////////
// Detect shake for erase calibration memory feature
///////////////////////////////////////////////////////////////////
bool detectShake() {
    const float SHAKE_THRESHOLD = 1.25; // Adjust threshold for sensitivity
    const unsigned long SHAKE_DURATION = 5000; // Shake time in milliseconds

    static unsigned long shakeStartTime = 0;
    static unsigned long lastShakeTime = 0;
    static bool isShaking = false;

    // Sensor variables
    float aX = 0, aY = 0, aZ = 0;

    // Read accelerometer values
    if (IMU.accelAvailable()) {
        IMU.readRawAccel(aX, aY, aZ);

        // Calculate total G-force
        float totalG = sqrt(aX * aX + aY * aY + aZ * aZ);
        totalG = abs(totalG); // Absolute value to handle negatives

        if (totalG > SHAKE_THRESHOLD) {
            lastShakeTime = millis(); // Update the last shake time
            if (!isShaking) {
                shakeStartTime = millis(); // Start shake timer
                isShaking = true;
                digitalWrite(LED_BUILTIN, HIGH);
            } else if (millis() - shakeStartTime >= SHAKE_DURATION) {
                return true; // Shake detected
            }
        } else if (millis() - lastShakeTime > 1000) { // 1000ms buffer to prevent immediate reset
            isShaking = false;
            setDataLedState(false);

            // Reset to normal
            setColorLedState("purple");
        }
    }
    return false;
}