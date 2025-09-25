# Vibrotactile Device for Psychophysical Experiments

This repository contains the hardware design, algorithms, and scripts developed for the vibrotactile device described in **Chapter 4** of the doctoral thesis *Design Practices for Encoding Abstract Data Using Vibrotactile Patterns*.  
The device was conceived as an experimental platform to support controlled user studies on vibrotactile perception, including thresholds, scales, and data encoding tasks.

---

## 📌 Overview
The vibrotactile device provides:
- Delivery of controlled vibrotactile patterns using multiple actuators (ERM motors).
- Support for frequency, duration, rhythm, and spatial arrangements.
- Integration with Arduino/ESP32 microcontrollers.
- Button interface for user responses.
- Modular design for experimental reproducibility.

---

## 📂 Repository Structure

## ⚙️ Requirements
- **Microcontroller:** Arduino Mega 2560 or ESP32 (depending on the experiment)
- **Actuators:** ERM vibration motors (tested up to 150 Hz)
- **Power supply:** 5V regulated
- **Additional components:** Buttons, resistors, breadboard/PCB
- **Software:** 
  - Arduino IDE (>= 2.0) or PlatformIO
  - Python 3.x (for data processing scripts)

---
## 🔌 System Architecture

+-------------------+       +-------------------+       +------------------+
|   Computer (PC)   | <---> |   Microcontroller | --->  | Motor Driver(s)  |
|   Data Logging    |  USB  | (Arduino / ESP32) | PWM   | + Vibration      |
+-------------------+       +-------------------+       |   Motors         |
             ^                     |   ^                +------------------+
             |                     |   |
             |     Serial Data     |   | Button Inputs
             |                     |   |
             +---------------------+   +----------------+
                                      User Response Buttons

---

## 🚀 Getting Started
1. Clone this repository:
   ```bash
   git clone https://github.com/<your-username>/vibrotactile-device.git
2.Open the firmware/ directory in Arduino IDE.
3.Upload the code to your microcontroller.
4.Assemble the hardware following the diagrams in hardware/.
5.Run the example experiment located in experiments/.

📜 License
This project is released under the MIT License. See LICENSE for details.



