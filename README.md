# DIY Magnet-Based Space Mouse (Modified Version)

> 💡 **Note:** This project is a modified version and improvement of the original [sb-ocr / diy-spacemouse](https://github.com) repository created by salimbenbouz (Instructables project: ["DIY Space Mouse for Fusion 360 Using Magnets"](https://instructables.com)).

<p align="center">
  <img src="Images/spacemouse.png" alt="Finished Space Mouse" width="60%" />
</p>

## 🛠️ Enhancements & Modifications

### 1. Hardware & Design Updates
* **Solid Lead Weight Base:** Modified the `Base_bottom` design to accommodate a solid lead disc (Blei-Scheibe) mounted from the bottom instead of using loose steel BBs. This ensures a clean assembly and stable center of gravity.
* **Complete 3D Data:** Provided the modified base plate in multiple formats (STEP, STL, OBJ, 3MF) inside the `/hardware` folder to support both direct 3D printing and further CAD customizations.

### 2. Firmware Updates (Adafruit QT Py)
* **Auto-Reinitialization & Monitoring:** Added continuous sensor health monitoring via the `TLx493D` library. If the magnetometer stalls or encounters errors, the firmware automatically re-initializes the sensor without requiring a reboot.
* **Watchdog Protection:** Integrated the `Adafruit_SleepyDog` library to force an automated hardware reset if the main loop ever completely stalls or freezes.
* **Signal Filtering:** Added a `SimpleKalmanFilter` to smooth out axis values and eliminate sensor jitter.

## 📂 Repository Structure
* `/hardware` - Modified 3D models & print files (STEP, STL, OBJ, 3MF format)
* `/Images` - Photos of the modification and finished build
* `/firmware` - Updated Arduino sketch for Adafruit QT Py

## 📋 Bill of Materials (BOM)

To build this modified magnet-based Space Mouse, you will need the following electronic and mechanical components. The links lead directly to the items I used for my build on AliExpress:

| Component | Qty | Description / Specifications | Link |
| :--- | :---: | :--- | :--- |
| **Adafruit QT Py** | 1 | Microcontroller board used as the main HID input device | [View on AliExpress](https://s.click.aliexpress.com/e/_c3DRNdEn)* |
| **TLV493D / TLx493D** | 1 | 3-Axis Magnetometer Sensor (I2C / STEMMA QT) | [View on AliExpress](https://s.click.aliexpress.com/e/_c3DRNdEn)* |
| **STEMMA QT Cable** | 1 | JST SH 4-pin cable (approx. 100mm) to connect sensor and MCU | [View on AliExpress](https://s.click.aliexpress.com/e/_c3DRNdEn)* |
| **Tactile Buttons** | 2 | 6mm micro tactile switch buttons for side shortcuts | [View on AliExpress](https://s.click.aliexpress.com/e/_c3ZY0dkF)* |
| **LilyPad LEDs** | 1 | Small LED boards used for internal case illumination / status | [View on AliExpress](https://aliexpress.com)* |
| **Neodymium Magnets** | 4 | Strong round magnets (usually 10x3mm or according to design) | [View on AliExpress](https://s.click.aliexpress.com/e/_c4C58nEn)* |
| **Spring Kit** | 1 | Compression and extension springs for the joystick tension | [View on AliExpress](https://s.click.aliexpress.com/e/_c4BEdeZD)* |
| **Lead Weight Disc** | 1 | Solid 60mm diameter (3-3.5mm thick) lead disc for a heavy base | [View on AliExpress](https://s.click.aliexpress.com/e/_c37fQ8nH)* |
| **PETG Filament** | — | Filament for 3D printing the enclosure components (e.g., Kingroon)| [View on AliExpress](https://s.click.aliexpress.com/e/_c3EpvDFp)* |

---

### 📢 Transparency Note & Support
Product links marked with an asterisk (`*`) are **affiliate links**. If you purchase through these links on AliExpress, I receive a small commission from the seller. There are **absolutely no extra costs** for you. By using these links, you directly support the maintenance and further development of this open-source project. Thank you!

---

## 🔧 Setup & Installation

### 1. Hardware & Lead Disc Specifications
* **Base Cutout Dimensions:** The bottom cutout has a diameter of **61 mm** and a depth of **3.5 mm**.
* **Recommended Lead Disc Size:** A disc with a diameter (**d**) of **60 mm** and a thickness (**h**) of **3.0 to 3.5 mm** is recommended.
* **Assembly:** Print the modified base plate from the `/hardware` folder. The lead disc can be easily secured inside the bottom cutout using **double-sided adhesive tape** (doppelseitiges Klebeband).

<p align="center">
  <img src="Images/Base_bottom_modified.png" alt="Modified Base with Lead Weight" width="50%" />
</p>

### 2. Firmware Compilation
Before flashing the code from the `/firmware` folder using the Arduino IDE, make sure to install the following dependencies via the Arduino Library Manager:

* **TinyUSB Mouse and Keyboard** (Ensure the USB stack in your Arduino IDE menu under *Tools -> USB Stack* is set to *TinyUSB*)
* **OneButton** (by Matthias Hertel)
* **TLx493D** (Infineon Technologies 3D Magnetic Sensor library)
* **SimpleKalmanFilter** (by Denys Sene)
* **Adafruit SleepyDog Library** (by Adafruit)

## 📄 License & Attribution
This project contains modifications and extensions based on the work of salimbenbouz ([sb-ocr/diy-spacemouse](https://github.com)). In accordance with the original project's terms, this work is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)** License. You are free to share and adapt it for non-commercial purposes, provided you give appropriate credit and distribute your contributions under the same license.
