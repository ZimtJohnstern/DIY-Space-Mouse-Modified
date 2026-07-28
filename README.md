# DIY Magnet-Based Space Mouse (Modified Version)

This repository is a modified fork and improvement of the original ["DIY Space Mouse for Fusion 360 Using Magnets"](https://www.instructables.com/DIY-Space-Mouse-for-Fusion-360-Using-Magnets/) by salimbenbouz, based on the [original source code](https://github.com/sb-ocr/diy-spacemouse). 

## 🛠️ Enhancements & Modifications

### 1. Hardware & Design Updates
* **Solid Lead Weight Base:** Modified the `Base_bottom` design to accommodate a solid lead disc (Blei-Scheibe) mounted from the bottom instead of using loose steel BBs. This ensures a clean assembly and stable center of gravity.
* **Complete 3D Data:** Provided the modified base plate in multiple formats (STEP, STL, OBJ, 3MF) to support both direct 3D printing and further CAD customizations.

### 2. Firmware Updates (Adafruit QT Py)
* **Auto-Reinitialization & Monitoring:** Added continuous sensor health monitoring via the `TLx493D` library. If the magnetometer stalls or encounters errors, the firmware automatically re-initializes the sensor without requiring a reboot.
* **Watchdog Protection:** Integrated the `Adafruit_SleepyDog` library to force an automated hardware reset if the main loop ever completely stalls or freezes.
* **Signal Filtering:** Added a `SimpleKalmanFilter` to smooth out axis values and eliminate sensor jitter.

## 📂 Repository Structure
* `/hardware` - Modified 3D models & print files (STEP, STL, OBJ, 3MF format)
* `/firmware` - Updated Arduino sketch for Adafruit QT Py

## 🔧 Setup & Installation

### 1. Hardware Assembly
1. Print the modified base plate from the `/hardware` folder (3MF/STL formats available).
2. Mount your lead disc to the bottom of the printed base.

### 2. Firmware Compilation
Before flashing the code from the `/firmware` folder using the Arduino IDE, make sure to install the following dependencies via the Arduino Library Manager:

* **TinyUSB Mouse and Keyboard** (Ensure the USB stack in your Arduino IDE menu under *Tools -> USB Stack* is set to *TinyUSB*)
* **OneButton** (by Matthias Hertel)
* **TLx493D** (Infineon Technologies 3D Magnetic Sensor library)
* **SimpleKalmanFilter** (by Denys Sene)
* **Adafruit SleepyDog Library** (by Adafruit)

## 📄 License & Attribution
This project is based on the work of salimbenbouz. In accordance with the original project's terms, this work is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)** License. You are free to share and adapt it for non-commercial purposes, provided you give appropriate credit.
