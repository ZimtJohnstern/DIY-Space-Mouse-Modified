# DIY Magnet-Based Space Mouse (Modified Version)

This repository is a modified fork and improvement of the original ["DIY Space Mouse for Fusion 360 Using Magnets"](https://instructables.com) by salimbenbouz, based on the [original source code](https://github.com). 

<p align="center">
  <img src="hardware/Images/spacemouse.png" alt="Finished Space Mouse" width="45%" />
  <img src="hardware/Images/Base_bottom_modified.png" alt="Modified Base with Lead Weight" width="45%" />
</p>

## 🛠️ Enhancements & Modifications

### 1. Hardware & Design Updates
* **Solid Lead Weight Base:** Modified the `Base_bottom` design to accommodate a solid lead disc (Blei-Scheibe) mounted from the bottom instead of using loose steel BBs. This ensures a clean assembly and stable center of gravity.
* **Complete 3D Data:** Provided the modified base plate in multiple formats (STEP, STL, OBJ, 3MF) inside the `/hardware/CAD` folder to support both direct 3D printing and further CAD customizations.

### 2. Firmware Updates (Adafruit QT Py)
* **Auto-Reinitialization & Monitoring:** Added continuous sensor health monitoring via the `TLx493D` library. If the magnetometer stalls or encounters errors, the firmware automatically re-initializes the sensor without requiring a reboot.
* **Watchdog Protection:** Integrated the `Adafruit_SleepyDog` library to force an automated hardware reset if the main loop ever completely stalls or freezes.
* **Signal Filtering:** Added a `SimpleKalmanFilter` to smooth out axis values and eliminate sensor jitter.

## 📂 Repository Structure
* `/hardware/CAD` - Modified 3D models & print files (STEP, STL, OBJ, 3MF format)
* `/hardware/Images` - Photos of the modification and finished build
* `/firmware` - Updated Arduino sketch for Adafruit QT Py

## 🔧 Setup & Installation

### 1. Hardware & Lead Disc Specifications
* **Base Cutout Dimensions:** The bottom cutout has a diameter of **61 mm** and a depth of **3.5 mm**.
* **Recommended Lead Disc Size:** A disc with a diameter (**d**) of **60 mm** and a thickness (**h**) of **3.0 to 3.5 mm** is recommended.
* **Assembly:** Print the modified base plate from the `/hardware/CAD` folder. The lead disc can be easily secured inside the bottom cutout using **double-sided adhesive tape** (doppelseitiges Klebeband).

### 2. Firmware Compilation
Before flashing the code from the `/firmware` folder using the Arduino IDE, make sure to install the following dependencies via the Arduino Library Manager:

* **TinyUSB Mouse and Keyboard** (Ensure the USB stack in your Arduino IDE menu under *Tools -> USB Stack* is set to *TinyUSB*)
* **OneButton** (by Matthias Hertel)
* **TLx493D** (Infineon Technologies 3D Magnetic Sensor library)
* **SimpleKalmanFilter** (by Denys Sene)
* **Adafruit SleepyDog Library** (by Adafruit)

## 📄 License & Attribution
This project is based on the work of salimbenbouz. In accordance with the original project's terms, this work is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)** License. You are free to share and adapt it for non-commercial purposes, provided you give appropriate credit.
