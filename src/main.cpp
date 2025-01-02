#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include <Fusion/Fusion.h>
#include <Calibration/Nano33_Calibration.h>

//ENABLE OR DISABLE CALIBRATION LOADING/SAVING/APPLYING
const bool EnableCalibration = true;

//ENABLE OR DISABLE TEST MODE (dev stuff)
const bool TestMode = false;

// LED Pin definitions
#define PIN_LED     (13u)
#define LED_BUILTIN PIN_LED
#define RED        (22u)
#define GREEN      (23u)
#define BLUE       (24u)
#define LED_PWR    (25u)

//Timing Code
unsigned long loopCounter = 0;
const long RequiredMicros = 5000;
unsigned long previousMicros = 0;
unsigned long previousSecond = 0;

// IMU Sample Rate for Fusion AHRS
#define SAMPLE_RATE (100)

// Initialise algorithms
FusionOffset offset;
FusionAhrs ahrs;

// IO Init Tracker
bool IOInitDone = false;

// Hatire Data Structure
struct {
    int16_t Begin;        // Start marker
    uint16_t Cpt;         // Counter
    float gyro[3];        // Gyroscope data (Pitch, Roll, Yaw)
    float acc[3];         // Accelerometer data
    int16_t End;          // End marker
} hat = {
    (int16_t)0xAAAA,      // Start marker
    0,                    // Counter
    {0.0f, 0.0f, 0.0f},   // Gyroscope data
    {0.0f, 0.0f, 0.0f},   // Accelerometer data
    (int16_t)0x5555       // End marker
};

// BLE variables + init
bool BLEconnected = false;
// BLE Service and Characteristic UUIDs
const char* deviceServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214";
const char* deviceCharacteristicUuid = "19b10001-e8f2-537e-4f6c-d104768a1214";

// BLE Service and Characteristic
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
// Sensor Fusion Init and Config
///////////////////////////////////////////////////////////////////
void setupFusion(){

    if (EnableCalibration) {
        if (!loadCalibrationData()) {
            runCalibrationSequence();
            saveCalibrationData();
        }

        // Apply loaded calibration data
        gyroscopeMisalignment = calibrationData.gyroscopeMisalignment;
        gyroscopeSensitivity = calibrationData.gyroscopeSensitivity;
        gyroscopeOffset = calibrationData.gyroscopeOffset;
        accelerometerMisalignment = calibrationData.accelerometerMisalignment;
        accelerometerSensitivity = calibrationData.accelerometerSensitivity;
        accelerometerOffset = calibrationData.accelerometerOffset;
        //softIronMatrix = calibrationData.softIronMatrix;
        //hardIronOffset = calibrationData.hardIronOffset;
    }

    FusionOffsetInitialise(&offset, SAMPLE_RATE);
    FusionAhrsInitialise(&ahrs);

    // Set AHRS algorithm settings
    const FusionAhrsSettings settings = {
            .convention = FusionConventionNwu,
            .gain = 0.55f, //.5 default
            .gyroscopeRange = 2000.0f, /* replace this with actual gyroscope range in degrees/s */
            .accelerationRejection = 15.0f,
            .magneticRejection = 15.0f, //10 default
            .recoveryTriggerPeriod = 1 * SAMPLE_RATE, /* 5 seconds */
    };
    FusionAhrsSetSettings(&ahrs, &settings);
}

///////////////////////////////////////////////////////////////////
// Update angles from IMU
///////////////////////////////////////////////////////////////////
void updateAngles() {
    if (IMU.gyroAvailable()) {
        IMU.readRawGyro(gX, gY, gZ);
        gX *= -1.0; // Apply calibration and flip X axis
    }

    if (IMU.accelAvailable()) {
        IMU.readRawAccel(aX, aY, aZ);
        aX *= -1.0; // Flip X-axis to match orientation
    }

    if (IMU.magneticFieldAvailable()) {
        IMU.readRawMagnet(mX, mY, mZ);
    }

    const clock_t timestamp = clock(); // replace this with actual gyroscope timestamp
    FusionVector gyroscope = {gX, gY, gZ}; // in degrees/s
    FusionVector accelerometer = {aX, aY, aZ}; // in g
    FusionVector magnetometer = {mX, mY, mZ}; // in arbitrary units

    // Apply calibration
    gyroscope = FusionCalibrationInertial(gyroscope, gyroscopeMisalignment, gyroscopeSensitivity, gyroscopeOffset);
    if (isnan(gyroscope.axis.x) || isnan(gyroscope.axis.y) || isnan(gyroscope.axis.z)) {
        Serial.println("Error: Gyroscope calibration resulted in NaN!");
        return; // Skip this update
    }
    accelerometer = FusionCalibrationInertial(accelerometer, accelerometerMisalignment, accelerometerSensitivity, accelerometerOffset);
    if (isnan(accelerometer.axis.x) || isnan(accelerometer.axis.y) || isnan(accelerometer.axis.z)) {
        Serial.println("Error: Accelerometer calibration resulted in NaN!");
        return; // Skip this update
    }
    magnetometer = FusionCalibrationMagnetic(magnetometer, softIronMatrix, hardIronOffset);
    if (isnan(magnetometer.axis.x) || isnan(magnetometer.axis.y) || isnan(magnetometer.axis.z)) {
        Serial.println("Error: Magnetometer calibration resulted in NaN!");
        return; // Skip this update
    }

    // Update gyroscope offset correction algorithm
    gyroscope = FusionOffsetUpdate(&offset, gyroscope);

    // Calculate delta time (in seconds) to account for gyroscope sample clock error
    static clock_t previousTimestamp;
    const float deltaTime = (float) (timestamp - previousTimestamp) / (float) CLOCKS_PER_SEC;
    previousTimestamp = timestamp;

    // Update gyroscope AHRS algorithm
    FusionAhrsUpdate(&ahrs, gyroscope, accelerometer, magnetometer, deltaTime);

    // Set algorithm outputs
    const FusionEuler euler = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));
    //const FusionVector earth = FusionAhrsGetEarthAcceleration(&ahrs);

    if (TestMode) {
        // TestMode CODE
        static unsigned long lastPrintTime = 0; // Timer for printing
        const unsigned long printInterval = 500; // Print every N ms
        unsigned long currentMillis = millis();
        if (currentMillis - lastPrintTime >= printInterval) {
            lastPrintTime = currentMillis;

            //Print the header only once
            static bool headerPrinted = false;
            if (!headerPrinted) {
                Serial.println("   Pitch      Roll       Yaw");
                Serial.println("-------------------------------");
                headerPrinted = true;
            }

            // Print the Euler angles in columns
            Serial.print("   ");
            Serial.print(euler.angle.pitch, 2);
            Serial.print("    ");
            Serial.print(euler.angle.roll, 2);
            Serial.print("    ");
            Serial.println(euler.angle.yaw, 2);

            /* Raw data
            Serial.print("Raw Gyro: ");
            Serial.print(gX);
            Serial.print(", ");
            Serial.print(gY);
            Serial.print(", ");
            Serial.print(gZ);
            
            Serial.print("   Calibrated Gyro: ");
            Serial.print(gyroscope.axis.x);
            Serial.print(", ");
            Serial.print(gyroscope.axis.y);
            Serial.print(", ");
            Serial.println(gyroscope.axis.z);

            Serial.print("Raw Accel: ");
            Serial.print(aX);
            Serial.print(", ");
            Serial.print(aY);
            Serial.print(", ");
            Serial.print(aZ);

            Serial.print("   Calibrated Accel: ");
            Serial.print(accelerometer.axis.x);
            Serial.print(", ");
            Serial.print(accelerometer.axis.y);
            Serial.print(", ");
            Serial.println(accelerometer.axis.z);*/
        }
    } else {
        hat.gyro[0] = euler.angle.yaw;
        hat.gyro[1] = -euler.angle.pitch;
        hat.gyro[2] = -euler.angle.roll;
        sendAnglesToHatire();
    }
}

///////////////////////////////////////////////////////////////////
// Init IO
///////////////////////////////////////////////////////////////////
void initIO() {

    Serial.println("Starting BLE initialization...");

    // Initialize BLE
    if (!BLE.begin()) {
        Serial.println("[ERROR] BLE initialization failed! Is the BLE hardware functional?");
        while (1); // Halt execution
    } else {
        Serial.println("[INFO] BLE initialization successful.");
    }
    delay(100);
    // Set device local name
    BLE.setLocalName("Nano 33 Head Tracker");
    Serial.println("[INFO] Set local name to 'Nano 33 Head Tracker'.");
    delay(100);
    // Set advertised service
    BLE.setAdvertisedService(opentrackService);
    Serial.println("[INFO] Advertised service linked to BLE peripheral.");
    delay(100);
    // Add characteristic
    opentrackService.addCharacteristic(hatireCharacteristic);
    Serial.println("[INFO] Added characteristic to the service.");
    delay(100);
    // Add service to BLE peripheral
    BLE.addService(opentrackService);
    Serial.println("[INFO] Service added to BLE peripheral.");
    delay(100);
    // Initialize characteristic with default data
    if (!hatireCharacteristic.setValue((byte *)&hat, sizeof(hat))) {
        Serial.println("[ERROR] Failed to set initial value for characteristic!");
    } else {
        Serial.println("[INFO] Initial value set for characteristic.");
    }
    delay(100);

    // Start advertising
    if (!BLE.advertise()) {
        Serial.println("[ERROR] BLE advertising failed to start!");
    } else {
        Serial.println("[INFO] BLE advertising started successfully.");
    }

    delay(100);
    if (!TestMode) {
        Serial.begin(115200);
    }

    // Indicate IO initialization complete
    digitalWrite(BLUE, LOW);
    digitalWrite(RED, LOW);
    IOInitDone = true;
    Serial.println("[INFO] IO initialization complete.");
}

// Setup function
void setup() {

    //Hatire Bytes
    hat.Begin = 0xAAAA;
    hat.Cpt = 0;
    hat.End = 0x5555;

    //Set LEDs to output
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(RED, OUTPUT);
    pinMode(BLUE, OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(LED_PWR, OUTPUT);

    //Set default LED states (high/low is inverted for the color LEDs)
    digitalWrite(LED_PWR, LOW);
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, HIGH);
    digitalWrite(BLUE, HIGH);
    digitalWrite(LED_BUILTIN, LOW);

    //Early Init for testing
    if (TestMode) {
        Serial.begin(115200);
        while (!Serial);
    }

    //Init File System
    myFS = new FileSystem_MBED();
    while (!myFS);

    if (IMU.begin() && myFS->init()) {
        Serial.println("LSM9DS1 IMU Connected.");
        IMU.setGyroODR(3);
        IMU.setAccelODR(3);
        IMU.setMagnetODR(5);
        IMU.setContinuousMode();

        //IO Init
        initIO();

        //Wait for serial or BLE conenection here
        //If calibration enabled, check if the user is shaking the device during this time
        while (!Serial && !BLE.central() && EnableCalibration){
            if (detectShake()) {
                Serial.println("Shake detected! Clearing calibration data...");
                digitalWrite(BLUE, HIGH);
                digitalWrite(GREEN, LOW);
                digitalWrite(RED, LOW);
                clearCalibrationData();
                delay(3000); // Add a delay to prevent repeated triggering
                digitalWrite(GREEN, HIGH);
                digitalWrite(BLUE, LOW);
                Serial.println("Restarting device...");
                delay(100); // Small delay to allow Serial messages to be sent
                NVIC_SystemReset(); // Restart the microcontroller 
            }
            delay(1000);
        };

        setupFusion();
        delay(250);
    } else {
        if (!IMU.begin()){
          Serial.println("Failed to initialize IMU!");
        } else {
          Serial.println("Failed to initialize FileSystem!");
        }
        digitalWrite(BLUE, HIGH);
        while (true) {
            digitalWrite(RED, LOW);
            delay(500);
            digitalWrite(RED, HIGH);
            delay(500);
        }
    }
}

// Main loop
void loop() {

    if (Serial || BLE.central()) {
        BLEDevice central = BLE.central();
        if (central.connected()) {
            BLEconnected = true;
            digitalWrite(RED, HIGH);
            digitalWrite(BLUE, HIGH);
            digitalWrite(LED_PWR, HIGH);
            while (central.connected()) {
              unsigned long currentMicros = micros();
              if (currentMicros - previousMicros >= RequiredMicros) {
                previousMicros = currentMicros;
                updateAngles();
              }
              loopCounter++;
            }
            Serial.println("Central device disconnected!");
            BLEconnected = false;
            delay(500);
            BLE.advertise(); // Restart advertising after disconnect
            digitalWrite(RED, LOW);
            digitalWrite(BLUE, LOW);
            digitalWrite(LED_PWR, LOW);
        } else {
            BLEconnected = false;
            BLE.stopAdvertise();
            digitalWrite(RED, HIGH);
            digitalWrite(BLUE, HIGH);
            digitalWrite(LED_PWR, HIGH);
            while (Serial) {
              unsigned long currentMicros = micros();
              if (currentMicros - previousMicros >= RequiredMicros) {
                previousMicros = currentMicros;
                updateAngles();
              }
              loopCounter++;
              BLE.advertise(); // Restart advertising after disconnect
              /*if (millis() - previousSecond >= 1000 && TestMode) { // Every second
                Serial.print("\tLoop Frequency: ");
                Serial.print(loopCounter);
                Serial.println(" Hz");
                loopCounter = 0;
                previousSecond = millis();
              }*/
            }
        }
    }
    delay(1000);
}
