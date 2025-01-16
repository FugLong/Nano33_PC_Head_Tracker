# Arduino Nano 33 PC Head Tracker (WIP)

## UPDATE:

### Version 2 Update Almost Ready for Release! 🚀

Performance is **fantastic!** The core functionality is ready in the **V2_Update branch**, but documentation, project plans, and 3D models for the wireless version enclosure are still in progress.

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
3. [Usage](#3-usage)
4. [Calibration](#4-calibration)
5. [Configuring Opentrack](#7-configuring-opentrack)
6. [LED Indicator Meanings](#8-led-indicator-meanings)

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
  - Protects the Arduino and provides a mounting spot.

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

3. **Adafruit MiniBoost (Booster Board)**:
   - Choose one of the following:
     - [**100 mA Version**](https://www.adafruit.com/product/3661): $3
       - Works for this project and most others under typical use. Slightly less efficient but adequate.
     - [**1A Version**](https://www.adafruit.com/product/4654): $4
       - Overpowered for this project but provides extra reliability.

4. **Lipo Battery**:
   - [Adafruit 400mah Lipo Battery](https://www.adafruit.com/product/3898): $6-$10
     - Maximum supported battery size: ~36mm x ~17mm x 7.8mm.
     - Ensure it has **over-discharge protection** or build the optional **battery monitoring circuit** to avoid damage.

5. **Slide Toggle Switch**: ~$0.50 (SS12D00)
   - Any of size SS12D00 will work
   - I bought [these from amazon](https://www.amazon.com/HiLetgo-SS-12D00-Toggle-Switch-Vertical/dp/B07RTJDW27)

6. Optional: **Battery Monitoring Circuit**:
   - **Resistors**: 150KΩ and 300KΩ. (can be ~100k + ~50k using 2 in place of 1)
   - **Capacitor**: ~0.22 µF. (can be really any size 1 µF or below)

---

## 7. Configuring Opentrack

### Steps:
1. Open **Opentrack** and set the **Input Dropdown** to `Hatire Arduino`.
2. Set your active COM port.
4. Ensure **DTR is enabled** in the settings menu.
5. Configure mapping:
   - *(Optional)* Adjust curves for smoother tracking.

---

## 8. LED Indicator Meanings

| **Color**            | **State**                               |
|----------------------|-----------------------------------------|
| **Red**              | Booting up.                             |
| **Purple**           | Waiting for connection.                 |
| **Flashing Red**     | Something is going wrong, maybe hardware|
| **Power LED Green**  | Functioning as intended.                |
| **Green + Orange**   | Uploading script.                       |
| **Dark Blue**        | Calibrating Gyro, hold still            |
| **Green**            | Calibrating Accel, rotate smoothly      |
| **Cyan/Light Blue**  | Calibrating Mag, figure 8               |

---

