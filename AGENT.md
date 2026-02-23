# Project Overview: Solar Flower

This project implements an Arduino-based solar tracker, named "Solar Flower," designed for efficient solar energy collection with a focus on extreme power saving. The system utilizes photoresistors to detect the strongest light source and adjusts the position of a solar panel (simulated by the "flower") using two servo motors: one for horizontal movement and one for vertical movement.

## Motivation

The primary motivation is to improve the accuracy and duration of a solar tracking system, building upon a commercial DIY kit. The key innovation is placing photo sensors on the back side of the photovoltaic cell, ensuring the sensors move in conjunction with the cell. This design aims to achieve more precise tracking by directing the entire system towards the light source based on differential light readings from the sensors.

## Hardware Setup

The project is built around an Arduino Uno and components from a KEYESTUDIO DIY Solar Tracking Electronic Kit.

**Key Components:**
- **Arduino Uno:** The microcontroller brain of the system.
- **Photoresistors (LDRs):** Four photoresistors (Up, Down, Left, Right) are used to detect light intensity. These are connected to analog pins A0-A3.
- **Servo Motors:** Two servo motors control the horizontal (pin 9) and vertical (pin 10) movement of the solar panel.
- **Power Deactivation Pins:** Digital pins 4 and 5 are used to deactivate power to the photoresistors and servos, respectively, for power-saving purposes.

The photo sensors are strategically placed on the backside of the photovoltaic cell to ensure they move with the cell, enabling the system to "follow" the light source accurately.

## Software

The software, written in Arduino C (C++), focuses on:
- **Light Sensing:** Continuously reads light intensity from four photoresistors (Up, Down, Left, Right).
- **Servo Control:** Adjusts the horizontal and vertical angles of the solar panel based on light differentials.
- **Power Saving:** Utilizes the `LowPower` library to put the Arduino into deep sleep mode during periods of inactivity or low light to conserve power.
- **Calibration:** Includes logic for calibrating photoresistor readings, especially for the right sensor.
- **Error Handling:** Checks for disconnected photoresistors to prevent erroneous movements.
- **Serial Output:** Provides debug information and sensor readings via the serial monitor.

**Key Software Features:**
- **`setup()` function:** Initializes serial communication, sets up pin modes for sensors and actuators, reads initial sensor values, and sets actuators to initial positions.
- **`loop()` function:** The main execution loop, which repeatedly reads sensors, checks for error states, reasons about movement, controls actuators, and manages sleep states.
- **`readPhotoSensors()`:** Activates photoresistors, reads their analog values, and applies calibration.
- **`reasonAboutNextSteps()`:** Determines if and how the servos should move based on the photoresistor readings and defined error tolerances. It also manages sleep time based on light conditions.
- **`controlActuators()`:** Activates servo power and sets the servo angles if movement is required.
- **`handleSleep()`:** Manages the sleep cycle, utilizing `LowPower.powerDown()` for energy conservation.
- **`longSleep()`:** A helper function for `handleSleep()` to perform deep sleep for various durations.
- **`setServoHorizontalAngle()` and `setServoVerticalAngle()`:** Functions to control the specific angles of the horizontal and vertical servos using pulse width modulation.

## Power Saving Mechanism

The project incorporates a sophisticated power-saving mechanism:
- **Selective Power Deactivation:** Photoresistors and servo motors can be individually powered down when not in use.
- **Deep Sleep:** The `LowPower` library is used to put the Arduino into deep sleep mode for specified durations (`shortSleepTime` or `longSleepTime`). The duration of sleep is determined by whether the system is actively tracking strong light or in a low-light/inactive state.
- **`longSleepCount`:** Tracks consecutive long sleep periods to adjust error tolerance, preventing unnecessary movements after waking up.

## Future Outlook

The `README.md` indicates areas for future development, including further measurements and general outlook improvements. The current code provides a robust foundation for a power-efficient solar tracking system.
