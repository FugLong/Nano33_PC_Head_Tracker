#include <Arduino_LSM9DS1.h>

// LED Pin definitions
#define PIN_LED     (13u)
#define LED_BUILTIN PIN_LED
#define RED        (22u)
#define GREEN      (23u)
#define BLUE       (24u)
#define LED_PWR    (25u)

// Define Battery Monitoring Pin
#define BATTERY_PIN A7

// ENABLE OR DISABLE TEST MODE (dev stuff)
const bool TestMode = false;

// Battery Function Declarations
void updateBatteryLEDs(float batteryVoltage);
float readBatteryVoltage();
bool isBatteryMonitoringAvailable();
void enterLowPowerMode();
void checkBattery();


// General Function Declarations
void setupPins();
bool detectShake();
void setPowerLedState(bool On);
void setDataLedState(bool On);
void setColorLedState(String Color);
void logString(String input, bool newLine);
void logString(float input, bool newLine);

