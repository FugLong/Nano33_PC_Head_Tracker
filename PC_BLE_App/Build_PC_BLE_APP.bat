@echo off
:: Script to build the Nano33 Head Tracker app using PyInstaller

:: Change directory to the location of this batch file
cd /d "%~dp0"

:: Run PyInstaller with the spec file
pyinstaller "./Nano33 Head Tracker - PC BLE.spec"

:: Pause to view output if there are any errors
pause
