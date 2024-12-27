#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include <Fusion/Fusion.h>
#include <FS_Nano33BLE.h>

#define CALIBRATION_FILE MBED_FS_FILE_PREFIX "/calibration.dat"

// LED Pin definitions
#define PIN_LED     (13u)
#define LED_BUILTIN PIN_LED
#define RED        (22u)
#define GREEN      (23u)
#define BLUE       (24u)
#define LED_PWR    (25u)

// IMU Sample Rate for Fusion AHRS
#define SAMPLE_RATE (100)

//Timing Code
unsigned long loopCounter = 0;
const long RequiredMicros = 5000;
unsigned long previousMicros = 0;
unsigned long previousSecond = 0;

// Sensor variables
float gX = 0, gY = 0, gZ = 0;
float aX = 0, aY = 0, aZ = 0;
float mX = 0, mY = 0, mZ = 0;
float deltat;

// Initialise algorithms
FusionOffset offset;
FusionAhrs ahrs;

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

// FileSystem object
FileSystem_MBED *myFS;

// IO Init Tracker
bool IOInitDone = false;

// BLE variables + init
bool BLEconnected = false;
const char* deviceServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214";
const char* deviceServiceCharacteristicUuid = "19b10001-e8f2-537e-4f6c-d104768a1214";
BLEService opentrackService(deviceServiceUuid);
BLECharacteristic hatireCharacteristic(deviceServiceCharacteristicUuid, BLERead | BLEWrite | BLENotify, 30, true);

// Hatire structure
struct {
    int16_t Begin;
    uint16_t Cpt;
    float gyro[3];
    float acc[3];
    int16_t End;
} hat;

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

            // Print Accelerometer Calibration
            Serial.print("Accel Offsets: ");
            Serial.print(calibrationData.accelerometerOffset.axis.x);
            Serial.print(", ");
            Serial.print(calibrationData.accelerometerOffset.axis.y);
            Serial.print(", ");
            Serial.println(calibrationData.accelerometerOffset.axis.z);

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
    Serial.println("Calibrating gyroscope... Place the device stationary and do not move it.");
    float gXSum = 0, gYSum = 0, gZSum = 0;
    int samples = 0;

    for (int i = 0; i < 500; i++) { // Collect 500 samples
        if (IMU.gyroAvailable()) {
            IMU.readRawGyro(gX, gY, gZ);
            gXSum += gX;
            gYSum += gY;
            gZSum += gZ;
            samples++;
        }
        delay(10); // 10ms delay for 100Hz sampling
    }

    calibrationData.gyroscopeOffset.axis.x = gXSum / samples;
    calibrationData.gyroscopeOffset.axis.y = gYSum / samples;
    calibrationData.gyroscopeOffset.axis.z = gZSum / samples;

    Serial.println("Gyroscope calibration complete.");
    Serial.print("Offsets: ");
    Serial.print(calibrationData.gyroscopeOffset.axis.x);
    Serial.print(", ");
    Serial.print(calibrationData.gyroscopeOffset.axis.y);
    Serial.print(", ");
    Serial.println(calibrationData.gyroscopeOffset.axis.z);
}

///////////////////////////////////////////////////////////////////
// Accel Calibration
///////////////////////////////////////////////////////////////////
void calibrateAccelerometer() {
    Serial.println("Calibrating accelerometer... Rotate the device to ensure each face points upward.");

    float aMin[3] = {999, 999, 999}; // Minimum readings for X, Y, Z
    float aMax[3] = {-999, -999, -999}; // Maximum readings for X, Y, Z

    unsigned long startTime = millis();
    while (millis() - startTime < 20000) { // Collect data for 20 seconds
        if (IMU.accelAvailable()) {
            IMU.readRawAccel(aX, aY, aZ);
            if (aX < aMin[0]) aMin[0] = aX;
            if (aX > aMax[0]) aMax[0] = aX;
            if (aY < aMin[1]) aMin[1] = aY;
            if (aY > aMax[1]) aMax[1] = aY;
            if (aZ < aMin[2]) aMin[2] = aZ;
            if (aZ > aMax[2]) aMax[2] = aZ;
        }
        delay(10);
    }

    calibrationData.accelerometerOffset.axis.x = (aMax[0] + aMin[0]) / 2.0f;
    calibrationData.accelerometerOffset.axis.y = (aMax[1] + aMin[1]) / 2.0f;
    calibrationData.accelerometerOffset.axis.z = (aMax[2] + aMin[2]) / 2.0f;

    calibrationData.accelerometerSensitivity.axis.x = 2.0f / (aMax[0] - aMin[0]);
    calibrationData.accelerometerSensitivity.axis.y = 2.0f / (aMax[1] - aMin[1]);
    calibrationData.accelerometerSensitivity.axis.z = 2.0f / (aMax[2] - aMin[2]);

    Serial.println("Accelerometer calibration complete.");
    Serial.print("Offsets: ");
    Serial.print(calibrationData.accelerometerOffset.axis.x);
    Serial.print(", ");
    Serial.print(calibrationData.accelerometerOffset.axis.y);
    Serial.print(", ");
    Serial.println(calibrationData.accelerometerOffset.axis.z);
    Serial.print("Sensitivity: ");
    Serial.print(calibrationData.accelerometerSensitivity.axis.x);
    Serial.print(", ");
    Serial.print(calibrationData.accelerometerSensitivity.axis.y);
    Serial.print(", ");
    Serial.println(calibrationData.accelerometerSensitivity.axis.z);
}

///////////////////////////////////////////////////////////////////
// Mag Calibration
///////////////////////////////////////////////////////////////////
void calibrateMagnetometer() {
    Serial.println("Calibrating magnetometer... Move the device in a figure-eight pattern.");

    float mMin[3] = {999, 999, 999}; // Minimum readings for X, Y, Z
    float mMax[3] = {-999, -999, -999}; // Maximum readings for X, Y, Z

    unsigned long startTime = millis();
    while (millis() - startTime < 20000) { // Collect data for 20 seconds
        if (IMU.magneticFieldAvailable()) {
            IMU.readRawMagnet(mX, mY, mZ);
            if (mX < mMin[0]) mMin[0] = mX;
            if (mX > mMax[0]) mMax[0] = mX;
            if (mY < mMin[1]) mMin[1] = mY;
            if (mY > mMax[1]) mMax[1] = mY;
            if (mZ < mMin[2]) mMin[2] = mZ;
            if (mZ > mMax[2]) mMax[2] = mZ;
        }
        delay(10);
    }

    calibrationData.hardIronOffset.axis.x = (mMax[0] + mMin[0]) / 2.0f;
    calibrationData.hardIronOffset.axis.y = (mMax[1] + mMin[1]) / 2.0f;
    calibrationData.hardIronOffset.axis.z = (mMax[2] + mMin[2]) / 2.0f;

    float deltaX = (mMax[0] - mMin[0]) / 2.0f;
    float deltaY = (mMax[1] - mMin[1]) / 2.0f;
    float deltaZ = (mMax[2] - mMin[2]) / 2.0f;

    calibrationData.softIronMatrix.element.xx = 1.0f / deltaX;
    calibrationData.softIronMatrix.element.yy = 1.0f / deltaY;
    calibrationData.softIronMatrix.element.zz = 1.0f / deltaZ;

    Serial.println("Magnetometer calibration complete.");
    Serial.print("Hard Iron Offset: ");
    Serial.print(calibrationData.hardIronOffset.axis.x);
    Serial.print(", ");
    Serial.print(calibrationData.hardIronOffset.axis.y);
    Serial.print(", ");
    Serial.println(calibrationData.hardIronOffset.axis.z);
    Serial.println("Soft Iron Matrix: ");
    Serial.print(calibrationData.softIronMatrix.element.xx);
    Serial.print(", ");
    Serial.print(calibrationData.softIronMatrix.element.yy);
    Serial.print(", ");
    Serial.println(calibrationData.softIronMatrix.element.zz);
}

///////////////////////////////////////////////////////////////////
// Run Calibration for Gyro, Accel, and Mag
///////////////////////////////////////////////////////////////////
void runCalibrationSequence() {
    Serial.println("Starting calibration sequence...");
    //GYRO STAGE BLUE
    digitalWrite(BLUE, LOW); // Turn on blue LED to indicate calibration
    digitalWrite(RED, HIGH); // Turn off red LED
    calibrateGyroscope();

    //ACCEL STAGE GREEN
    digitalWrite(BLUE, HIGH);//Blue off
    digitalWrite(GREEN, LOW);//Green on
    calibrateAccelerometer();

    //MAG STAGE LIGHT BLUE
    digitalWrite(BLUE, LOW);//Blue ON
    calibrateMagnetometer();
    digitalWrite(GREEN, HIGH);//Green off
    digitalWrite(BLUE, HIGH);  // Turn off blue LED to indicate success
    Serial.println("Calibration sequence completed.");
}

///////////////////////////////////////////////////////////////////
// Sensor Fusion Init and Config
///////////////////////////////////////////////////////////////////
void setupFusion(){

    if (!loadCalibrationData()) {
        runCalibrationSequence();
        saveCalibrationData();
    }

    FusionOffsetInitialise(&offset, SAMPLE_RATE);
    FusionAhrsInitialise(&ahrs);

    // Set AHRS algorithm settings
    const FusionAhrsSettings settings = {
            .convention = FusionConventionNwu,
            .gain = 0.5f,
            .gyroscopeRange = 2000.0f, /* replace this with actual gyroscope range in degrees/s */
            .accelerationRejection = 10.0f,
            .magneticRejection = 10.0f,
            .recoveryTriggerPeriod = 5 * SAMPLE_RATE, /* 5 seconds */
    };
    FusionAhrsSetSettings(&ahrs, &settings);

    // Apply loaded calibration data
    gyroscopeMisalignment = calibrationData.gyroscopeMisalignment;
    gyroscopeSensitivity = calibrationData.gyroscopeSensitivity;
    gyroscopeOffset = calibrationData.gyroscopeOffset;
    accelerometerMisalignment = calibrationData.accelerometerMisalignment;
    accelerometerSensitivity = calibrationData.accelerometerSensitivity;
    accelerometerOffset = calibrationData.accelerometerOffset;
    softIronMatrix = calibrationData.softIronMatrix;
    hardIronOffset = calibrationData.hardIronOffset;
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
    accelerometer = FusionCalibrationInertial(accelerometer, accelerometerMisalignment, accelerometerSensitivity, accelerometerOffset);
    magnetometer = FusionCalibrationMagnetic(magnetometer, softIronMatrix, hardIronOffset);

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

    hat.gyro[0] = euler.angle.yaw;
    hat.gyro[1] = euler.angle.pitch;
    hat.gyro[2] = euler.angle.roll;
    sendAnglesToHatire();
}

///////////////////////////////////////////////////////////////////
// Detect shake for erase calibration memory feature
///////////////////////////////////////////////////////////////////
bool detectShake() {
    const float SHAKE_THRESHOLD = 1.5; // Adjust threshold for sensitivity
    const unsigned long SHAKE_DURATION = 5000; // Shake time in milliseconds

    static unsigned long shakeStartTime = 0;
    static unsigned long lastShakeTime = 0;
    static bool isShaking = false;

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
        } else if (millis() - lastShakeTime > 250) { // 250ms buffer to prevent immediate reset
            isShaking = false;
            digitalWrite(LED_BUILTIN, LOW);
        }
    }

    return false;
}

///////////////////////////////////////////////////////////////////
// Init IO
///////////////////////////////////////////////////////////////////
void initIO(){
    
    Serial.begin(115200);

    BLE.begin();
    BLE.setLocalName("Nano 33 Head Tracker");
    BLE.setAdvertisedService(opentrackService);
    opentrackService.addCharacteristic(hatireCharacteristic);
    BLE.addService(opentrackService);
    BLE.advertise();

    digitalWrite(BLUE, LOW);
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

    /*initIO();
    IOInitDone = true;
    while (!Serial);*/

    //Init File System
    myFS = new FileSystem_MBED();
    while (!myFS);

    if (IMU.begin() && myFS->init()) {
        Serial.println("LSM9DS1 IMU Connected.");
        IMU.setGyroODR(3);
        IMU.setAccelODR(3);
        IMU.setMagnetODR(3);
        IMU.setContinuousMode();

        setupFusion();
        digitalWrite(RED, LOW);
        digitalWrite(LED_PWR, LOW);
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

    if (!IOInitDone){
        initIO();
        IOInitDone = true;
    }

    if (detectShake()) {
        Serial.println("Shake detected! Clearing calibration data...");
        digitalWrite(BLUE, HIGH);
        digitalWrite(GREEN, LOW);
        digitalWrite(RED, LOW);
        clearCalibrationData();
        delay(3000); // Add a delay to prevent repeated triggering
        digitalWrite(GREEN, HIGH);
        digitalWrite(BLUE, LOW);
        /*Serial.println("Restarting device...");
        delay(100); // Small delay to allow Serial messages to be sent
        NVIC_SystemReset(); // Restart the microcontroller */
    }
    
    delay(1000);

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
              /**if (millis() - previousSecond >= 1000 && TestMode) { // Every second
                Serial.print("\tLoop Frequency: ");
                Serial.print(loopCounter);
                Serial.println(" Hz");
                loopCounter = 0;
                previousSecond = millis();
              }*/
            }
        }
    }
}
