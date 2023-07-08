# Arduino Nano 33 PC Head Tracker (WIP)

#### Fusion filter by [Aster94](https://github.com/aster94/SensorFusion), modified LSM9DS1 library by [Femme Verbeek]( https://www.linkedin.com/in/femmeverbeek/ ), hatire implementation used for reference by [juanmcasillas](https://github.com/juanmcasillas/HATino)
#### Extra special thanks to the [opentrack community](https://github.com/opentrack/opentrack) and original hatire plugin creator
#### Head tracker implementation and hatire/opentrack integration by me, [FugLong]( https://www.linkedin.com/in/elijah-stephenson-38a0a518b/ )

----------------------------------------------------------------------------
##### Click left pic to see a video of the tracker in action

<a href="http://www.youtube.com/watch?feature=player_embedded&v=pOG3jK_D8DQ" target="_blank">
  <img src="http://img.youtube.com/vi/pOG3jK_D8DQ/0.jpg" width=50% height=50%>
</a>
<img src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/938f0e7f-4039-4de8-8df7-3c636f73c521" width=40% height=40%>

links:

[Opentrack Releases](https://github.com/opentrack/opentrack/releases) | [Arduino Editor DL](https://www.arduino.cc/en/software) | [VS Code Download](https://code.visualstudio.com/download) | [Geomagnetic Calculator of the NOAA](https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml?#igrfwmm) | [Femme Verbeek's Calibration Docs](https://github.com/FemmeVerbeek/Arduino_LSM9DS1) | [Femme Verbeek's Calibration Instruction Video](https://youtu.be/BLvYFXoP33o) | [3D Printable Holder for Nano 33](https://www.thingiverse.com/thing:6111323)

----------------------------------------------------------------------
## 1. Introduction   

Welcome to the land of cheap head tracking! Why spend upwards of $150 when you can do it with a $25 Arduino?

This project is based around the LSM9DS1 IMU, specifically targeting the arduino nanos that use this IMU. It has decent (for now) head tracking, integration with opentrack so you can use it in virtually any game, and requires no coding or soldering. Bluetooth mode is currently supported but requires an extra PC program and is not yet optimised. In the future I plan to try out some wifi based solutions but that's not a main priority currently. 

## 2. Prerequisites 

First thing's first, you need an arduino with an LSM9DS1 and a usb cord. I know this works on the Nano 33 BLE, and sense versions at least. If you have other hardware and are code savvy then you should have no problem porting this to other IMUs/Ardys. If that's the case just grab the head tracker from the examples folder and ignore everything else.

Otherwise, you will need to start by grabbing some links from the link section or down below. 
  - Opentrack software
    - Head to the releases page [here](https://github.com/opentrack/opentrack/releases) and download the latest installer/executable for your OS.
    - This is an open source PC program that widely supports head tracking in virtually any game. We won't need this until we're done flashing our board.
  - Arduino Editor or VS Code
    - Go to the arduino software page [here](https://www.arduino.cc/en/software) or the VS code download page [here](https://code.visualstudio.com/download) and download the latest version for your OS.
    - This is what we'll use to install this library, its dependencies, and flash our calibration programs and head tracking code to our arduino.
  - This repo
    - If using Arduino editor, download this repo as a zip, we will be installing it as a library.
    - If you want to use VS code, clone this repo with git and open in vscode. 
   
These things aren't prerequisites but are worth having:
  - Velcro Tape
    - I got some very sticky velcro tape from amazon for like $10. It is very helpful when calibrating and when attaching the device to my headset.
  - A small box
    - This can be a small 3d printed box (I plan to make a 3d printable one soon and will link here), or even the paper box your arduino came in (that's what I used).
    - This is for calibration only, if you have the hands of a surgeon maybe you could go without it but it is very helpful.
  - 3D Printed Holder
    - [This](https://www.thingiverse.com/thing:6111323) is a 3d printed sleeve for the arduino that helps protect it as well as create a perfect spot to attach sticky tape/velcro. (also in the links section)
    - In the future I have plans on creating another version with room for a battery for fully wireless operation. Will link here when done.
      
## 3. Setup (Arduino Editor)

Start by booting up Arduino Editor.

#### **VERY IMPORTANT:**
In the Library Manager tab on the left bar, you must make sure you do **NOT** have the library "Arduino_LSM9DS1" by Arduino installed.

Now Go to **Sketch>Include Library>Add .ZIP Library** and point it where you downloaded this repo. It should install the library and ask you if you want to install dependencies. Say yes. 

<img width="359" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/c301be93-5ce6-4fb3-b31a-2b5ac4b811e0">
<img width="359" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/06831fe8-7291-495a-9f08-180402a92d0f">

If it doesn't prompt you to install dependencies, click on the left Library Manager tab and search for "ArduinoBLE" and "SensorFusion" and add them.

------------------

Next, click on board manager, the button above the library manager. You need to install "Arduino Mbed OS Nano Boards" by Arduino. Then click on select board on the top bar and select your board (it should be plugged in).

<img width="350" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/0ebc6117-e0c4-4a5b-8c26-8dcf9cb3f326">
<img width="500" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/24477d61-ddf4-4a8a-a737-04159339aa90">

-------------------

The last arduino editor specific setup topic is where to find the calibration and head tracking code, and how to upload it to the board.

To load the various files you'll need, go to **File>Examples>Nano33 PC Head Tracker>**. When time comes to upload to your board, press the big upload button on the top left (it has an arrow pointing to the right).

<img width="450" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/71e769bf-6738-4994-92d9-7d9f1d8167d5">
<img width="450" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/8555b8e8-6e99-4297-aa15-78958425431d">

## 3. Setup (VS Code)

WIP
