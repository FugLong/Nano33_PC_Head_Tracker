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
- **Automated persistent calibration**: No manual tuning required.
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
- **3D Printed Wired Case**: [Get it here.](https://www.thingiverse.com/thing:6111323)
  - **Only recommended for wired builds**
  - Protects the Arduino and provides a mounting surface.

---

### Wireless Version Required Hardware:

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
(Wireless version only) WIP

### Steps:
**1. Print yourself (or acquire through dubious means) the following 3d printed parts:**
   1. Enclosure/box and lid 
      - (also includes the disassembly tool)
   2. Mounting plate for _YOUR SPECIFIC HARDWARE [(Prerequisites)](#2-prerequisites)_ 
      - (including the tiny switch cover plate)

**2. Insert electrionics into mounting plate**
   1. Slide the Arduino into place on the top of the mounting plate
   2. Slide the charger and booster boards into place on the back side of the mounting plate
   3. Also place the toggle switch into place and slide the switch cover plate in to lock it in

**3. (Optional) Build the battery monitoring circuit.**
   1. Yes it's just a voltage reduction circuit so the Arduino doesn't get fried
   2. Maybe place the parts into the 3d printable jig I haven't made yet, or follow the video that doesn't exist yet to make it by hand?

**4. Wire Everything Up**
   1. Connect everything as shown in the wiring diagram I haven't digitized yet, or follow along in that video (not yet made)

**5. Plug the battery in and test**
   1. Flip the switch and make sure the lights turn on.
   2. If you already flashed the Arduino, give the ble connection a test too.

**6. Finish Assembly**
   1. Insert the whole package into the enclosure, **ensuring the switch aligns with the gap in the box**.
   2. Make sure it's aligned perfectly then push it all the way in, press hard.
   3. To put the lid on, squeeze the tabs and insert the corners into the center of the enclosure.
   4. Then you can push the lid flat and it should lock into place.

### ***If you need to disassemble it for any reason:***
   1. Use the tool to pop the lid's tabs by inserting the flat edge into the slots on top and bottom of the box
   2. Use the edge with 90 degree angles as a key to push the mounting plate out of the box from in between the USB ports.

---

## 4. Setup-Calibration

### Flashing the Arduino:
1. Open the Nano33 PC Head Tracker app you downloaded above.
2. Plug your arduino into your PC, the app should update to "Connected".
3. Press the flash arduino button, it will pull latest and flash the board for you.
4. Wait till it's done then you're ready to go.
   - This only needs to be done once per update/settings change.
   - If a new script update is released the UI will notify you that you should reflash.

### First Boot/Calibration:
1. On first boot (or after calibration data has been cleared) the device will start a calibration sequence.
   - If you need to restart during calibration just press the little reset button on the ardy.
2. Dark Blue = Gyro calibration. Keep the device **perfectly** still. ~10 sec.
3. Green = Accel calibration. Rotate the device smoothly over all axis ~20 sec.
4. Flashing Light Blue/Cyan = Mag calibration. Move the device in figure-8 patterns to calibrate. ~30 sec.
5. After the cyan light stops flashing and goes solid this means the calibration is complete and will save to memory.
   - Then the device should automatically restart and go into idle mode (purple LED).

### Recalibration:
***If the device is acting up and/or you have moved locations you may want to recalibrate. Any time the device is in idle mode you can shake it violently for 5 seconds until the LED flashes yellow/orange. It will then restart and begin the calibration sequence after a few seconds.***

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

#### ***Then you're ready to start opentrack tracking and start gaming. Recenter in open track after initial 15 sec or so after the device has initialized.***

### Battery Monitoring Circuit:
1. If you installed the battery monitoring circuit, you can shake the device once while in idle mode to indicate current battery level.
2. It will show either green, yellow, red, or flashing red.
3. If at any time during init or operation the device detects that the battery voltage is below the set safe level, it will flash the red light and power down.

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
