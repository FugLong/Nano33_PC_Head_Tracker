# Arduino Nano 33 PC Head Tracker (WIP)

### Version 2 Update Almost Ready for Release! 🚀

Performance is **fantastic!** The core functionality is ready in the **V2_Update branch**, but documentation, project guides, and 3D models for the wireless version enclosure are still in progress.

#### Quick Crash Course:
1. Pull the **V2 branch**.
2. Install **Rust** and use `cargo` to run `/pc_app/`.
3. Connect your Arduino to your PC, then:
   - Press the **Flash Arduino** button in the app to install the latest firmware.
   - Use **Hatire Opentrack Plugin** (USB) or the **UDP Plugin** for BLE data via the PC App.
4. **Calibrate** your device:
   - On the first connection, set the device on a flat surface.
   - Follow the LED instructions for calibration:
     - **Dark Blue**: Keep perfectly still.
     - **Green**: Rotate the device smoothly around all axes.
     - **Cyan/Light Blue**: Perform figure-8 motions for magnetometer calibration.
   - **Clearing Calibration**: While LED is purple (powered but not connected), shake the device violently for **5 seconds** to reset.
5. Leave the device still for **10-15 seconds** on first connection to let filters stabilize. Then recenter in Opentrack.

## Contents:
1. [Introduction](#1-introduction)
2. [Prerequisites](#2-prerequisites)
3. [Assembly](#3-assembly)
4. [Setup/Calibration](#4-setup-calibration)
5. [Usage](#5-usage)
6. [Configuring Opentrack](#6-configuring-opentrack)
7. [LED Indicator Meanings](#7-led-indicator-meanings)

---

## 1. Introduction

Welcome to affordable head tracking! 🎮  
Why spend $150+ when you can build your own for just $25?

This project leverages the **Arduino Nano 33 BLE**, using the **LSM9DS1 IMU**, offering:
- **Great performance**: Low-latency head tracking for gaming.
- **One-click Arduino flashing**: Easy setup.
- **Automated calibration**: No manual tuning required.
- **Wireless capability**: Tether-free gaming.
- **Opentrack integration**: Works with virtually any game.

**Cost to build**:
- **Wired Version**: ~$25
- **Wireless Version**: ~$50  
  *(Estimated battery life: 10-18 hours with a 400 mAh battery.)*

---

## 2. Prerequisites

### Hardware:
- **Arduino Nano 33 BLE**: ~$25
- **Micro-USB Cable**

### Software:
- **Nano 33 Head Tracker PC App** *(Link incoming...)*
- **Opentrack Software**: [Get the latest release here.](https://github.com/opentrack/opentrack/releases)

### Recommended Accessories:
- **Velcro tape**: For mounting to headphones.

### Wired Version Recommendations:
- **3D Printed Wired Case**: [Get it here.](https://www.thingiverse.com/thing:6111323)
  - Protects the Arduino and provides a mounting surface.

---

### Wireless Version Additional Hardware:

1. **3D Printed Wireless Project Box** *(Link incoming...)*  
   - **IMPORTANT**: Download the **correct mounting plate** for your chosen **booster board** and **Micro Lipo Charger**.  
   - The enclosure includes:
     - A mounting plate for electronics.
     - A box with a lid.
     - A disassembly tool.

2. **Adafruit Micro Lipo Charger**:
   - Choose one of the following:
     - [USB-C Version](https://www.adafruit.com/product/4410): $6
     - [Micro-USB Version](https://www.adafruit.com/product/1904): $7

3. **Adafruit MiniBoost (5v)**:
   - Choose one of the following:
     - [**100 mA Version**](https://www.adafruit.com/product/3661): $3
       - Works for this project and most others under typical use. Slightly less efficient but adequate.
     - [**1A Version**](https://www.adafruit.com/product/4654): $4
       - Overpowered for this project but provides extra reliability.

4. **Lipo Battery**:
   - [Adafruit 400mah Lipo Battery](https://www.adafruit.com/product/3898): $6-$10
     - Maximum supported battery size: ~36mm x ~17mm x 7.8mm.
     - Ensure it has **over-discharge protection** and/or build the optional **battery monitoring circuit** to avoid damage.

5. **Slide Toggle Switch**: ~$0.50 (SS12D00)
   - Any of size SS12D00 will work
   - I bought [these from amazon](https://www.amazon.com/HiLetgo-SS-12D00-Toggle-Switch-Vertical/dp/B07RTJDW27)

6. Optional: **Battery Monitoring Circuit**:
   - **Resistors**: 150KΩ and 300KΩ. (can be ~100k + ~50k using 2 in place of 1)
   - **Capacitor**: ~0.22 µF. (can be really any size 1 µF or below)

---

## 3. Assembly
(Wireless version only)
Section WIP (obviously)

### Steps:
1. Print da ting
2. put the tings in da ting
3. solder up da tings
4. connect battery and test
5. put it in the box and put the lid on

---

## 4. Setup-Calibration

### Flashing the Arduino:
1. Open the Nano33 PC Head Tracker app you downloaded above.
2. Plug your arduino into your PC, the app should update to "Connected".
3. Press the flash arduino button, it will pull latest and flash the board for you.
4. Wait till it's done.
5. This only needs to be done once per update/settings change.
6. If a new script update is released the UI will notify you that you should reflash.

### First Boot/Calibration:
1. On first boot (or after calibration data has been cleared) the device will start a calibration sequence. If you need to restart during calibration just press the little reset button on the ardy.
2. Dark Blue = gyro calibration. Keep the device perfectly still. ~10 sec.
3. Green = accel calibration. Rotate the device smoothly over all axis ~20 sec.
4. Flashing Light Blue/Cyan = mag calibration. Move the device in figure-8 patterns to calibrate. ~30 sec.
5. After the cyan light stops flashing and goes solid this means the calibration is complete and will save to memory. Then the device should automatically restart and go into idle mode (purple LED).

#### ***If the device is acting up and/or you have moved locations you may want to recalibrate. Any time the device is in idle mode you can shake it violently for 5 seconds until the LED flashes yellow/orange. It will then restart and begin the calibration sequence after a few seconds.***

---

## 5. Usage
(Ensure you have completed Assembly/Setup and the device is attached to your headphones before getting to this point)

### Wired Usage Guide:
1. Open opentrack
2. Select the `Hatire Arduino` opentrack plugin as input. (Ensure **DTR is enabled** in settings)
3. Plug in arduino

### Wireless Usage Guide:
1. Open the **Nano33 PC Head Tracker app** and start the BLE loop
2. Power Arduino, it will auto connect to the PC app
3. Open **opentrack**
4. Select `UDP Over Network` opentrack plugin as input.

#### Then you're ready to start opentrack tracking and start gaming. Recenter in open track after initial 15 sec or so after the device has initialized.

---

## 6. Configuring Opentrack
There a a number of things I recommend doing to improve the feel and performance in opentrack. None of this is required, just what I like using.

### Filtering:
1. I like the `NaturalMovement` filter the best. (update to latest opentrack version if you don't see it)
2. I turn up rotation responsiveness nearly all the way up, and decrease the deadzones as much as possible

### Curves:
1. I recommend adding curves very similar to the ones included below. (pics incoming)
2. This allows for very smooth and non-distracting movement when close to center, while also allowing rotation easily and quickly when you give it more input.

### Output:
1. For most games, `Freetrack 2.0 Enhanced` will be your best best.
2. I'd recommend opening the settings and setting "Interface" to `Enable both`.

---

## 7. LED Indicator Meanings

| **Color**            | **State**                               |
|----------------------|-----------------------------------------|
| **Red**              | Booting up                              |
| **Purple**           | Waiting for connection                  |
| **Power LED Green**  | Functioning as intended                 |
| **Dark Blue**        | Calibrating Gyro, hold still            |
| **Green**            | Calibrating Accel, rotate smoothly      |
| **Cyan/Light Blue**  | Calibrating Mag, figure 8               |
| **Flashing Red**     | Battery dead or hardware fault          |
| **Green + Orange**   | Uploading script                        |

---
