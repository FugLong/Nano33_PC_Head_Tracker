#include <Calibration/Nano33_Calibration.h>
#include <Arduino_LSM9DS1.h>
#include <Fusion/Fusion.h>
#include <Utils/Utils.h>
#include <IO/IO.h>

//ENABLE OR DISABLE CALIBRATION LOADING/SAVING/APPLYING
//Recommended to leave enabled
const bool EnableCalibration = true;

// Set AHRS algorithm settings | TWEAK THE SETTINGS HERE TO CONTROL HOW THE TRACKING FEELS/BEHAVES
const FusionAhrsSettings settings = {
        .convention = FusionConventionNwu,
        .gain = 0.75f, //.5 initial default (adds responsiveness at the expense of increased error, good to find a balance)
        .gyroscopeRange = 2000.0f, // gyroscope range in degrees/s (Don't need to modify in most cases)
        .accelerationRejection = 12.0f, //10 inital default (this ignores more problematic accel data, helping smooth things out at the expense of latency and potentially accuracy if turned too high))
        .magneticRejection = 12.0f, //10 initial default (this ignores more problematic mag data, helping smooth things out at the expense of latency and potentially accuracy if turned too high)
        .recoveryTriggerPeriod = 1000, 
        /* 5 seconds initial default - 5000
        * Recommended Range (50-10000 ms) 
        * 
        * Set to smaller values right now (1000 ms) to increase fast movement accuracy and speed up error recovery. This is at the expense of stability at non-center angles for longer periods of time. 
        * Basically this is trying to recenter and correct for error after the device hasn't moved much in this amount of time. 
        * 
        * Setting it to 1000 or even lower is best for fast paced fighter pilot type movement, where you aren't holding the same non-center head angle perfectly for more than a few seconds at a time. This gives you near perfect accuracy.
        * 
        * If you do look away and stay still for too long the device will start recentering automatically at this new false center.
        * On the bright side, if this value is low this new false center will get corrected out pretty quick once you look forward again. This can still be annoying for certain playstyles.
        * 
        * If you aren't using this for fast paced action and want to be able to have more stability for sightseeing and holding angles for long periods of time, or just want more of a balance,
        * try increasing this value to as high as you like. 1000 ms is my personal choice
        */
};

// Initialise algorithms
FusionOffset offset;
FusionAhrs ahrs;

// Sensor variables
float gX = 0, gY = 0, gZ = 0;
float aX = 0, aY = 0, aZ = 0;
float mX = 0, mY = 0, mZ = 0;
float deltat;

//Timing Code
unsigned long loopCounter = 0;
const long RequiredMicros = 5000;
unsigned long previousMicros = 0;
unsigned long previousSecond = 0;

// IMU Sample Rate for Fusion AHRS
#define SAMPLE_RATE (100)

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

    //Applies the settings from the top of the script
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
                logString("   Pitch      Roll       Yaw", true);
                logString("-------------------------------", true);
                headerPrinted = true;
            }

            // Print the Euler angles in columns
            logString("    ", false);
            logString(euler.angle.pitch, false);
            logString("    ", false);
            logString(euler.angle.roll, false);
            logString("    ", false);
            logString(euler.angle.yaw, true);
        }
    } else {
        hat.gyro[0] = -euler.angle.yaw; //Yaw
        hat.gyro[1] = euler.angle.pitch; //Actually Roll
        hat.gyro[2] = -euler.angle.roll; //Actually Pitch
        sendAnglesToHatire();
    }
}

// Setup function
void setup() {

    //Set up LED control and battery monitoring pins
    setupPins();

    //Early serial init for testing
    if (TestMode) {
        Serial.begin(115200);
        while (!Serial);
        delay(2000);
    }

    //Battery check to ensure it's not dead on boot
    checkBattery();

    //Init filesystem for calibration data loading and saving
    initFS();

    if (IMU.begin() && myFS->init()) {
        logString("LSM9DS1 IMU Connected.", true);
        IMU.setGyroODR(3);
        IMU.setAccelODR(3);
        IMU.setMagnetODR(5);
        IMU.setContinuousMode();

        //IO Init
        initIO();

        setupFusion();
        delay(250);
    } else {
        if (!IMU.begin()){
          logString("Failed to initialize IMU!", true);
        } else {
          logString("Failed to initialize FileSystem!", true);
        }
        digitalWrite(BLUE, HIGH);
        while (true) {
            setColorLedState("off");
            delay(500);
            setColorLedState("red");
            delay(500);
        }
    }
}

// Main loop
void loop() {

    //Wait for serial or BLE conenection here
    //If calibration enabled, check if the user is shaking the device during this time
    while (!Serial && !BLE.central() && EnableCalibration){
        if (detectShake()) {
            logString("Shake detected! Clearing calibration data...", true);
            setColorLedState("orange");
            clearCalibrationData();
            delay(3000); // Add a delay to prevent repeated triggering
            setColorLedState("purple");
            logString("Restarting device...", true);
            delay(100); // Small delay to allow Serial messages to be sent
            NVIC_SystemReset(); // Restart the microcontroller 
        }
        delay(1000);
    }

    if (Serial || BLE.central()) {

        BLEDevice central = BLE.central();
        if (central.connected()) {
            BLEconnected = true;
        } else {
            BLEconnected = false;
        }

        BLE.stopAdvertise();
        setColorLedState("off");
        setPowerLedState(true);

        while (Serial || central.connected()) {
          unsigned long currentMicros = micros();
          if (currentMicros - previousMicros >= RequiredMicros) {
            previousMicros = currentMicros;
            updateAngles();
            loopCounter++;
          }
          if (millis() - previousSecond >= 10000) { // Every 10 seconds

            checkBattery();
            loopCounter = 0;
            previousSecond = millis();

            /*logString("\tLoop Frequency: ", false);
            logString(loopCounter/10, false);
            logString(" Hz", true);*/
          }
        }

        //On serial or BLE disconnect -
        logString("Device disconnected!", true);
        BLEconnected = false;
        setColorLedState("purple");
        setPowerLedState(false);
        delay(500);
        BLE.advertise(); // Restart advertising after disconnect
    }
    delay(1000);
}
