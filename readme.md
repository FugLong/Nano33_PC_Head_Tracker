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

[Opentrack Releases](https://github.com/opentrack/opentrack/releases) | [Arduino Editor DL](https://www.arduino.cc/en/software) | [VS Code Download](https://code.visualstudio.com/download) | [Geomagnetic Calculator of the NOAA](https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml?#igrfwmm) | [Femme Verbeek's Calibration Docs](https://github.com/FemmeVerbeek/Arduino_LSM9DS1) | [Femme Verbeek's Calibration Instruction Video](https://youtu.be/BLvYFXoP33o) | [3D Printable Holder for Nano 33](https://www.thingiverse.com/thing:6111323) | [3D Printable Calibration Box](https://www.thingiverse.com/thing:6113315) 

----------------------------------------------------------------------

## Contents: 	
1. Introduction
    * WIP Updates
2. Prerequisites
3. Setup (Arduino Editor)
4. Setup (VS Code)
5. Calibration
6. Flashing Head Tracking Code
7. Configuring Opentrack
8. LED Indicator Meanings

## 1. Introduction   

Welcome to the land of cheap head tracking! Why spend upwards of $150 when you can do it with a $25 Arduino?

This project is based around the LSM9DS1 IMU, specifically targeting the arduino nanos that use this IMU. It has decent (for now) head tracking, integration with opentrack so you can use it in virtually any game, and requires no coding or soldering.

### WIP Updates: 

Currently there are drift issues I hope to resolve, but performance is still very good. For me it's comparable to single webcam based tracking, so keep a recenter button handy either in opentrack or in game. 

Currently I recommend disabling roll in opentrack settings. It has been giving me weird behavior and even affecting other axis. Once I figure out how to get it in line I'll update this doc.

Bluetooth is technically currently working but requires a python based bluetooth to serial program running on the PC that is lame and keeps breaking for me. If I optimize it more I will link the bluetooth to serial program. Also considering 2 arduino client server setup to get around the program and use real serial.

## 2. Prerequisites 

First thing's first, you need an arduino with an LSM9DS1 and a usb cord. I know this works on the Nano 33 BLE, and sense versions at least. If you have other hardware and are code savvy then you should have no problem porting this to other IMUs/Ardys. If that's the case just grab the head tracker from the examples folder and ignore everything else.

Otherwise, you will need to start by grabbing some links from the link section or down below. 
  - Opentrack software
    - Head to the releases page [here](https://github.com/opentrack/opentrack/releases) and download the latest installer/executable for your OS.
    - This is an open source PC program that widely supports head tracking in virtually any game. We won't need this until we're done flashing our board.
  - Arduino Editor or VS Code (more involved)
    - Go to the arduino software page [here](https://www.arduino.cc/en/software) or the VS code download page [here](https://code.visualstudio.com/download) and download the latest version for your OS.
    - This is what we'll use to install this library, its dependencies, and flash our calibration programs and head tracking code to our arduino.
  - This repo
    - If using Arduino editor, download this repo as a zip, we will be installing it as a library.
    - If you want to use VS code, clone this repo with git and open in vscode. 
   
These things aren't prerequisites but are worth having:
  - Velcro Tape
    - I got some very sticky velcro tape from amazon for like $10. It is very helpful when calibrating and when attaching the device to my headset.
  - A small box
    - This can be a small 3d printed box like [this](https://www.thingiverse.com/thing:6113315) one I made, or even the paper box your arduino came in (that's what I used at first).
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

## 4. Setup (VS Code)

**The VS code setup will be less detailed as I expect mainly experienced users will use it.**

First, open up VS code and install the Arduino extension from the extension menu. When installing, select CLI on the bottom right, the gui will be removed soon. (Restart VS code to make sure the new UI elements show up)

Then, open up the nano 33 head tracking folder, either from a local git clone you've made or by downloading the zip and extracting.

All of the settings we need can be accessed by hitting the shortcut **Ctrl+Shift+P**. From the drop down select "Arduino: Board Manager" and a UI panel should open up. Install "Arduino Mbed OS Nano Boards" by Arduino. 

Next, hit Ctrl+Shift+P again and select "Arduino: Library Manager". From there, install "ArduinoBLE" and "SensorFusion". If you have "Arduino_LSM9DS1" installed make absolutely sure you remove it before proceeding. (If you have to delete it, restart VS code after)

Finally, at the very bottom of the screen on the right, there are options to select board, file you want to upload, and port. Make sure to set your board and port (arduino needs to be plugged in). Don't worry about selecting a programmer.

All the files you need are in the **examples** folder. To upload set the file you want on the bottom bar, then you can use the same drop down menu and select upload, or you can click the little icon in the top right that looks like an arrow pointing into a full bucket (may have to wait for it to compile and click it again).

## 5. Calibration

Calibration is done in 3 parts and can be finicky but thankfully you really only need to do it once. Femme Verbeek's fork of the Arduino_LSM9DS1 library adds easy calibration and he even made a [fantastic video](https://youtu.be/BLvYFXoP33o) showing how to do each type so his fork is what this repo is based on. 

If anything here isn't immediately clear, check out the tutorial video above to make sure you're doing it right.

To start, create a text file somewhere you'll remember and call it something like "headTracker_calibrationData". Leave it open.

---------------------

For the first calibration step upload the **"DIY_Calibration_Accelerometer"** ino to your board from the examples folder.

Once uploaded open up the serial monitor. In the arduino editor, it's the button on the far top right; in VS Code there's a tab for it where the terminal and output tabs are.

You should see the calibration dialogue. This one is the easiest. You need to put the board flat and completely still on it's back, front, top, bottom, left, and right. This is where the calibration box comes in. I have a 3d printable calibration box available in the links that makes this a lot easier, but you can just tape/velcro/glue the board inside of the box it came in and it works great. 

Once the device is stable and level, type C in the input and hit enter. You repeat this for all sides of the box. The program will autodetect which axis and direction you are doing and keeps track of which are complete.

Once all axis are OK, copy the "Accelerometer code" and paste it in your data text file. It should look something like this:

    // Accelerometer code
    IMU.setAccelFS(0);
    IMU.setAccelODR(2);
    IMU.setAccelOffset(-0.010573, -0.013723, -0.008323);
    IMU.setAccelSlope (0.995522, 1.013914, 1.005888);

----------------------

For the second calibration step upload the **"DIY_Calibration_Gyroscope"** ino to your board from the examples folder.

Just like before open the serial monitor and make sure you see the gryo calibration dialogue. This step has 2 parts. The first part is just like the previous calibration, but the second part is more involved.

To start, make sure your board is level and not moving at all on its back. Then type O in the input, hit enter, and wait for offsets to calibrate. That's the first part done.

Next, you will need to type C into the input, hit enter, smoothly rotate the device 180 degrees over just one axis, then hit enter again. This can be done by keeping one side of your calibration box on a table and rotating it around in a semi circle while keeping it on the table the whole time. This will need to be done 3 times, once for each axis. Check the video if this doesn't make sense right away.

Once this is done copy the "Gyroscope code" and paste it in your data text file below the accel code. It should look something like this:

    // Gyroscope code
    IMU.setGyroFS(1);
    IMU.setGyroODR(2);
    IMU.setGyroOffset (0.318680, -0.165939, 0.185928);
    IMU.setGyroSlope (1.163980, 1.116704, 1.132782);

-----------------------

For the third and final calibration step upload the **"DIY_Calibration_Magnetometer"** ino to your board from the examples folder.

Then, open serial monitor again and make sure you see the dialogue. 

This one is the weirdest. First off, go to [this](https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml?#igrfwmm) website and enter your approximate location on the right. It will convert to lat and lon and you can get local mag data. Look specifically at **Inclination** and **Total Field**. 

Now type L in the serial monitor, hit enter, and then type your local total field strength and hit enter again.

For this next part you need a compass or a compass app on your phone and I highly recommend watching the video for a visual aid. Open it up and align your phone with north. Then align your board with north and tilt it along your local inclination line like in the video. Now not going to lie I could not find where he got that print out in the video, so I just based my inclination angle roughly where it should be relative to the line in his video. 

Once it's in the proper orientation, type C into the input and hit enter. Then rotate it along all the axis while keeping it aligned with north and your inclination line. Once you have rotated all axis around 360, hit enter again to stop calibrating. 

Finally you are done and can copy the "Magnetometer code" and paste it in your data text file below the gyro code. It should look something like this:

    // Magnetometer code
    IMU.setMagnetFS(0);
    IMU.setMagnetODR(7);
    IMU.setMagnetOffset(-3.352051, -1.088867, 22.079468);
    IMU.setMagnetSlope (2.696329, 1.037236, 1.078650);

## 6. Flashing Head Tracking Code

You're on the home stretch now. You can close all of the calibration files if they're still open, except for your data text file, leave that open.

The last thing we need to flash to the arduino is the actual head tracking code, but first you need to paste your calibration data in.

Open up the **"Nano33_HeadTracker_v1"** ino file and scroll down to line 195, or where you see a big comment block that says "PASTE YOUR CALIBRATION DATA HERE".
There will be default dummy data there in the same format as your calibration text doc. Highlight everything within that comment and replace it with your data.

It should look something like this:

    //----------------------------------Set calibration data--------------------------------------------
    //----------------------------PASTE YOUR CALIBRATION DATA HERE--------------------------------------
    // Accelerometer code
    IMU.setAccelFS(0);
    IMU.setAccelODR(2);
    IMU.setAccelOffset(-0.010573, -0.013723, -0.008323);
    IMU.setAccelSlope (0.995522, 1.013914, 1.005888);

    // Gyroscope code
    IMU.setGyroFS(1);
    IMU.setGyroODR(2);
    IMU.setGyroOffset (0.318680, -0.165939, 0.185928);
    IMU.setGyroSlope (1.163980, 1.116704, 1.132782);

    // Magnetometer code
    IMU.setMagnetFS(0);
    IMU.setMagnetODR(7);
    IMU.setMagnetOffset(-3.352051, -1.088867, 22.079468);
    IMU.setMagnetSlope (2.696329, 1.037236, 1.078650);
    //--------------------------------------------------------------------------------------------------
    //--------------------------------------------------------------------------------------------------

Now you're done on the arduino side. Flash the ino to your board and you can close arduino editor/VS code. 

## 7. Configuring Opentrack

The last and final step is getting opentrack configured to listen to our arduino. 

First, open up opentrack and set the input dropdown to "Hatire Arduino".

Until I get a weird roll bug figured out, next step is clicking on the big opentrack options button, click on the output tab, and set roll to "Disabled". Roll is hit or miss right now so for reliability, just turn it off.

<img width="450" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/ed4b6a39-2716-44b8-b53a-668ed6535fc9">

Now click the settings button to the right of the hatire dropdown. In the first settings menu, make sure to set your active com port if it's not already selected.

On the "command" tab of the settings menu you can leave almost everything the same but you **HAVE** to make sure you check the "DTR" checkbox.

<img width="450" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/c4c8c50a-6f20-4b65-bbf9-5c7600267488">
<img width="450" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/86313e00-2eee-4bc6-b9aa-8dadd755f7bb">

--------------------

Lastly, click on the "Mapping" button in the main opentrack ui. Change yaw max input to 180, and same with pitch input and output. Next, set roll to 90. If you want you can add some points on the curves to add some deadzone or snappier movement near the max (deadzones really help with slight drift). You can now click start on opentrack and see how it works. If necessary use the hatire settings menu to invert axis to get them the right way for you. Stopping opentrack will stop the arduino code and put it back into waiting mode.

<img width="600" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/9ec8a02b-b683-455c-a0a6-1ccabecb1d87">
<img width="600" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/fe6680c6-c27c-49a8-8fc2-f228e981fb89">
<img width="600" alt="image" src="https://github.com/FugLong/Nano33_PC_Head_Tracker/assets/49841558/522d5d2d-8797-4a74-b3ea-28933660fdd9">

--------------------

Now you're done! You can pick any output type you want (I suggest track-ir for games that support it), click the start button on opentrack, and start gaming.

## 8. LED Indicator Meanings

  - Purple
    - Booting up
  - Red
    - Waiting for Bluetooth or Serial connection
  - Flashing Red
    - IMU not found or not working
  - Green power LED only
    - Functioning as intended, running tracking code
  - Green power LED + Orange data LED
    - Uploading script

