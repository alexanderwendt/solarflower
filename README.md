# Extreme Power Saving Solar Flower
Arduino code for a solar tracker

## Motivation
I wanted to create a solar flower that always turns to the direction of the light and make some additional energy to load the battery.
As the solar panel is small, the standby energy must be minimized to a minimum.


## Hardware Setup
From Amazon, I bought (KEYESTUDIO DIY Solar Tracking Electronic Kit for Arduino UNO IDE, Temperature and Humidity Sensor,BH1750 Light Sensor etc.Edu Programming Gift for Adults)[https://www.amazon.de/dp/B0B1NWBTS4?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1]
with an Arduino Uno and all necessary components. However, I wanted to improve the accuracy and duration of the system. 

To improve the accuracy, I put the photo sensors on the back side of the photovoltaic cell, to get the sensors to move with the photovoltaic cell. The principle behind it, is that 
the whole system shall move in the direction of the light. If the left photo sensor gets more light that the right sensor, then move to the left.

The circuit plan is presented in circuit.png.

A photo of the setup: breadboard_circuit.jpg

### Arduino
[AGENT] Reference the image
arduino top image: arduino_nano_upper_side.jpg
I removed the resistor for the power led to save energy

[AGENT] Reference the image
arduino bottom image: arduino_nano_bottom_side.jpg
I removed the dc-dc converter on the board for the 5V input. In that case, VIN cannot be used with higher voltages
without damaging the board. The only input allowed is over the 5V input and there, the voltage must be around 5V.


### Mosfet IRFZ44N
[AGENT] Reference the image
Image irfz44n.jpg

Purpose: In standby mode the sensors shall not consume any energy. Therefore, the ground part of the circuit 
is turned of with a signal 0 on D4. In case of signal 1, the mosfet lets the current pass to ground. 

[AGENT] Look at the related work and the videos there and the explain why we need resitors with the mosfet

### DC-DC Converter
[AGENT] Reference the image
image dc-dc_converter.jpg

Purpose: The input voltage from the solar converter is around 6.3V. For the arduino and the photo resistors, it shall be
5V. Therefore, I use a dc-dc converter from 6.3V to 5V for up to 100mA load with a very low extra power consumption. 

### Relay
[AGENT] Reference the image
image relay.jpg

Purpose: As the servos 


### Servos
Motors: for the horizontal turning, we use a 360° servo and for the vertical rotation, a 180° servo is used.

[AGENT] Add a placeholder for an image here:





## Software
solarflower.ino 


## Measurements
Standby power consumption: 0.3W.


## Outlook


## Related Work
[AGENT] Read the sources, make proper references of them and add relevant content to this readme. There reference shall contain
title, author, link and accessed on

Low Power Arduino! Deep Sleep Tutorial: https://youtube.com/watch?v=urLSDi7SD8M&amp;si=4CFFTlt0aQmxE5yj
Low Power Arduino - Sleeping at 0.3mA perfect for batteries (Arduino Nan...: https://youtube.com/watch?v=usKaGRzwIMI&amp;si=3ZH7sSvCxpVQYAgh
Arduino Nano: Alle Infos zum Stromverbrauch – CHIP: https://praxistipps.chip.de/arduino-nano-alle-infos-zum-stromverbrauch_101217
How to use Mosfet: https://www.youtube.com/watch?v=GDZC3B4w-Bg&ab_channel=Ingenieursmentalit%C3%A4t
Here is why MOSFET drivers are sometimes essential! || MOSFET Driver Par...: https://youtube.com/watch?v=8swJ_Bnsgl4&si=RQiKRE4quMlKPZRd
Arduino Relais ansteuern – Schaltplan und Sketch: https://iotspace.dev/arduino-relais-ansteuern-schaltplan-und-sketch/





