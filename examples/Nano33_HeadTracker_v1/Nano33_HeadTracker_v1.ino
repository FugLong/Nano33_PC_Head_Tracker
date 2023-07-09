#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include "SensorFusion.h"

// LED Pin definitions
// *RGB PINS ARE INVERTED ON THIS BOARD*
// for RED, BLUE, GREEN, and BUILTIN only: HIGH means LOW and vice versa.
#define PIN_LED     (13u)
#define LED_BUILTIN PIN_LED
#define RED        (22u)
#define GREEN        (23u)
#define BLUE        (24u)
#define LED_PWR     (25u)

//TEST MODE ENABLE OR DISABLE
bool TestMode = false; //Set true to output data directly to serial (bypass hatire conversion + range mapping)

//Variables only required in test mode
int loopFrequency = 0;
const long displayPeriod = 100;
unsigned long previousMillis = 0;

//Init filter
SF fusion;

//Init gyro vars
float gX = 0, gY = 0, gZ = 0;
//Init accel vars
float aX = 0, aY = 0, aZ = 0;
//Init mag vars
float mX = 0, mY = 0, mZ = 0;
//Init delta time
float deltat;

//BLE variables + init
bool BLEconnected = false;
const char* deviceServiceUuid = "19b10000-e8f2-537e-4f6c-d104768a1214";
const char* deviceServiceCharacteristicUuid = "19b10001-e8f2-537e-4f6c-d104768a1214";
BLEService opentrackService(deviceServiceUuid); 
BLECharacteristic hatireCharacteristic(deviceServiceCharacteristicUuid, BLERead | BLEWrite | BLENotify, 30, true);

//this structure is needed by hatire
struct  {
  int16_t  Begin;   // 2  Debut
  uint16_t Cpt;      // 2  Compteur trame or Code
  float    gyro[3];   // 12 [Y, P, R]    gyro (actually just the Y,P,R from the fusion filter)
  float    acc[3];    // 12 [x, y, z]    Acc (this is left empty and ignored)
  int16_t  End;      // 2  Fin
} hat;

////////Converts degrees to radians////////////
float degreesToRadians(float degreeIn) {
  return (degreeIn * 71) / 4068.0;
}

///////Converts G's to m/s^2//////////////
float gsToMss(float gIn) {
  return  gIn * 9.80665;
}


///////////////////////////////////////////////////////////////////
// Sends hat struct to hatire using current communication method //
///////////////////////////////////////////////////////////////////
void sendAnglesToHatire() {
  // Send HAT  Frame to  PC base on communication mode
  // if bluetooth mode
  if (BLEconnected) {
    hatireCharacteristic.writeValue((byte*)&hat,30);
  }
  // else serial mode
  else {
    Serial.write((byte*)&hat,30);
  }
  hat.Cpt++;
  if (hat.Cpt>999) 
  {
    hat.Cpt=0;
  }
}

///////////////////////////////////////////////////////
// Updates the IMU and fusion filter to get new data //
///////////////////////////////////////////////////////
void updateAngles() {
  // ------Check for new IMU data and update angles------
  //update gryo if available
  if (IMU.gyroAvailable()) {
    IMU.readGyro(gX, gY, gZ);
  }
  //update accel if available
  if (IMU.accelAvailable()) {
    IMU.readAccel(aX, aY, aZ);

  }
  //update mag if available
  if (IMU.magnetAvailable()) {
    IMU.readMagnet(mX, mY, mZ);
  }

  //Gyro and Accel conversions
  gX = degreesToRadians(gX);
  gY = degreesToRadians(gY);
  gZ = degreesToRadians(gZ);
  aX = gsToMss(aX);
  aY = gsToMss(aY);
  aZ = gsToMss(aZ);

  //get delta time
  deltat = fusion.deltatUpdate();

  //Update filter
  //fusion.MahonyUpdate(gX, gY, gZ, aX, aY, aZ, deltat);
  //fusion.MahonyUpdate(gX, gY, gZ, aX, aY, aZ, mX, mY, mZ, deltat);
  fusion.MadgwickUpdate(gX, gY, gZ, aX, aY, aZ, mX, mY, mZ, deltat);

  if (TestMode){
    //  Display sensor data every displayPeriod, non-blocking.
    if (millis() - previousMillis >= displayPeriod) {
      Serial.print("Pitch:");
      Serial.print(fusion.getRoll());
      Serial.print("\tRoll:");
      Serial.print(fusion.getPitch());
      Serial.print("\tYaw:");
      Serial.println(fusion.getYaw());
      /*
      Serial.print("\tLoop Frequency: ");
      Serial.print(loopFrequency);
      Serial.println(" Hz");
      */
      loopFrequency = 0;
      previousMillis = millis();
    }
    loopFrequency++;
  }
  else {
    //Assign yaw, pitch, and roll in hatire struct
    hat.gyro[0]=map(fusion.getYaw(), 0, 360, -180, 180); //Yaw in opentrack
    hat.gyro[1]=-fusion.getPitch(); //Roll in opentrack
    hat.gyro[2]=fusion.getRoll(); //Pitch in opentrack

    // Send HAT  Frame to  PC
    sendAnglesToHatire();
  }
}

//////////////////////////////////
// Setup function, runs at boot //
//////////////////////////////////
void setup() {
  // Set hatire constants //
  // header frame
  hat.Begin=0xAAAA;
  // Frame Number or err code
  hat.Cpt=0;
  // footer frame
  hat.End=0x5555;

  // initialize the led digital pins as an output
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(LED_PWR, OUTPUT);

  //Disable power LED until setup procedure is complete, Enable purple while booting up
  digitalWrite(LED_PWR, LOW);
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, HIGH);
  digitalWrite(BLUE, LOW);
  digitalWrite(LED_BUILTIN, LOW);

  //Bluetooth Init
  BLE.begin();
  BLE.setLocalName("Nano 33 Head Tracker");
  BLE.setAdvertisedService(opentrackService);
  opentrackService.addCharacteristic(hatireCharacteristic);
  BLE.addService(opentrackService);
  hatireCharacteristic.writeValue((byte*)&hat,30);
  BLE.advertise();

  //Serial and filter Start
  Serial.begin(115200);
  
  //If IMU will init
  if (IMU.begin()) {
    Serial.print("LSM9DS1 IMU Connected.\n"); 
  
    //Set boot up led RED only by disabling blue to indicate device is now running and waiting for connection
    digitalWrite(BLUE, HIGH);

    //----------------------------------Set calibration data--------------------------------------------
    //----------------------------PASTE YOUR CALIBRATION DATA HERE--------------------------------------
    // Accelerometer code
    IMU.setAccelFS(0);
    IMU.setAccelODR(2);
    IMU.setAccelOffset(0.000000, 0.000000, 0.000000);
    IMU.setAccelSlope (1.000000, 1.000000, 1.000000);

    // Gyroscope code
    IMU.setGyroFS(1);
    IMU.setGyroODR(2);
    IMU.setGyroOffset (0.000000, 0.000000, 0.000000);
    IMU.setGyroSlope (1.000000, 1.000000, 1.000000);

    // Magnetometer code
    IMU.setMagnetFS(0);
    IMU.setMagnetODR(7);
    IMU.setMagnetOffset(0.000000, 0.000000, 0.000000);
    IMU.setMagnetSlope (1.000000, 1.000000, 1.000000);
    //--------------------------------------------------------------------------------------------------
    //--------------------------------------------------------------------------------------------------
  }
  else {
    // LSM9DS1 IMU not found
    digitalWrite(BLUE, HIGH);
    //Flash Red Light To indicate IMU error
    while(1) {
      digitalWrite(RED, LOW);
      delay(500);
      digitalWrite(RED, HIGH);
    }

  }
}

//////////////////////////////////////////////////////////////
// Attemps connection over BT and usb serial over and over. //
// Once connected, runs main loop until disconnected.       //
//////////////////////////////////////////////////////////////
void loop() {
  //LED red, pwr LED off while disconnected
  digitalWrite(RED, LOW);
  digitalWrite(LED_PWR, LOW);

  //Attempt Serial and Bluetooth connection
  BLEDevice central = BLE.central();
  delay(500);

  //if connected with BT or serial, else restart loop
  if (Serial || central) {

    //If bluetooth mode
    if (central.connected()) {
      BLEconnected = true;
      digitalWrite(RED, HIGH);
      digitalWrite(LED_PWR, HIGH);
      //call main loop, runs while connected
      while(central.connected()) {
        updateAngles();
        //  Wait for new sample - 7 ms delay provides a ~100Hz sample rate / loop frequency
        delay(14);
      }
    }

    // Else usb serial mode
    else {
      BLEconnected = false;
      BLE.stopAdvertise();
      digitalWrite(RED, HIGH);
      digitalWrite(LED_PWR, HIGH);
      //call main loop, runs while connected
      while(Serial) {
        updateAngles();
        //  Wait for new sample - 7 ms delay provides a ~100Hz sample rate / loop frequency
        delay(14);
      }
    }
  }

}