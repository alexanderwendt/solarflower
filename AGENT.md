# Project Overview: Solar Flower

This project implements an Arduino-based solar tracker, named "Solar Flower," designed for efficient solar energy collection with a focus on extreme power saving. The system utilizes photoresistors to detect the strongest light source and adjusts the position of a solar panel (simulated by the "flower") using two servo motors: one for horizontal movement and one for vertical movement.

## Motivation

This project was born from a desire to create an efficient solar tracker, often referred to as a "solar flower," capable of continuously orienting a solar panel towards the strongest light source. The primary goal is to maximize energy harvesting for battery charging, even with a small solar panel. A critical aspect of this design is minimizing standby energy consumption to ensure the system's long-term autonomy and efficiency, preventing the tracker itself from consuming a significant portion of the energy it collects. This pursuit of "extreme power saving" is central to the project's design philosophy.

To improve accuracy, the photo sensors are placed on the back side of the photovoltaic cell, ensuring they move in conjunction with the cell. The principle is that the entire system moves in the direction of the light. If the left photo sensor receives more light than the right sensor, the system moves to the left.

## Hardware Setup

The project is built around an Arduino Nano and components from a KEYESTUDIO DIY Solar Tracking Electronic Kit. The circuit plan is presented in `doc/media/circuit.png`.

**Key Components and Modifications:**
- **Arduino Nano:**
    - Power LED resistor removed to prevent constant current draw, reducing overall power footprint.
    - On-board DC-DC converter removed. Power must now be supplied directly via the 5V pin with a precisely regulated 5V supply, bypassing converter inefficiencies.
- **Photoresistors (LDRs):** Four photoresistors (Up, Down, Left, Right) are used to detect light intensity. These are connected to analog pins A0-A3.
- **Mosfet IRFZ44N:** Used to turn off the ground part of the sensor circuit in standby mode (signal 0 on D4). When signal 1 is applied, the MOSFET allows current to pass to ground. Resistors are used with the MOSFET to manage gate capacitance, protect the driving circuit, and control switching speed.
- **DC-DC Converter:** Converts the 6.3V input from the solar converter to a regulated 5V for the Arduino and photoresistors, designed for low extra power consumption.
- **Relay:** Controls higher power loads like the servo motors with a low-power signal from the Arduino, isolating the Arduino's control circuitry from the servos' current demands.
- **Servo Motors:** Two servo motors for precise positioning. A 360° servo for horizontal tracking (azimuth) and a 180° servo for vertical adjustment (altitude). These are controlled by the Arduino but powered via the relay.
- **Power Deactivation Pins:** Digital pins 4 and 5 are used to deactivate power to the photoresistors and servos, respectively, for power-saving purposes.

The photo sensors are strategically placed on the backside of the photovoltaic cell to ensure they move with the cell, enabling the system to "follow" the light source accurately.

## Software

The software, written in Arduino C (C++), resides in `solarflower.ino` and focuses on:
- **Light Sensing:** Continuously reads light intensity from four photoresistors (Up, Down, Left, Right).
- **Directional Control:** Calculates the optimal direction for the solar panel by comparing light intensities and controlling two servo motors.
- **Power Management:** Implements advanced power-saving techniques, including deep sleep modes using the `LowPower` library, to significantly reduce energy consumption when the system is idle or during low light conditions. This includes deactivating power to sensors and servos when not in active use.
- **Calibration & Error Handling:** Incorporates calibration for photoresistor readings and checking for potential error states, such as disconnected sensors, to ensure reliable operation.
- **Serial Communication:** Provides serial output for debugging and monitoring sensor values and servo positions.

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
- **Selective Power Deactivation:** Photoresistors and servo motors can be individually powered down when not in use (via digital pins 4 and 5).
- **Deep Sleep:** The `LowPower` library is used to put the Arduino into deep sleep mode for specified durations (`shortSleepTime` or `longSleepTime`). The duration of sleep is determined by whether the system is actively tracking strong light or in a low-light/inactive state.
- **`longSleepCount`:** Tracks consecutive long sleep periods to adjust error tolerance, preventing unnecessary movements after waking up.

## Measurements
Standby power consumption: 0.3mW. This measurement is a critical achievement, demonstrating the effectiveness of implemented hardware and software optimizations (like MOSFET-controlled sensor power and Arduino sleep modes) in significantly reducing the overall energy footprint when the system is not actively tracking.

## Future Outlook

Future improvements could focus on further refining power consumption, integrating advanced tracking algorithms for cloudy conditions, or exploring alternative energy storage solutions.
