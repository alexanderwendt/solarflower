# Coding Style for Solar Flower

This document defines the coding style for the Solar Flower project to ensure consistency and readability across the codebase.

## General Principles
- **Readability first:** Write code that is easy to understand for others (and your future self).
- **Consistency:** Follow the established patterns in the codebase.
- **Power Efficiency:** Code should be written with power-saving in mind, avoiding busy-waits where possible.

## Formatting
- **Indentation:** Use 2 spaces for indentation. Do not use tabs.
- **Line Length:** Aim for a maximum of 100 characters per line.
- **Braces:** Use K&R style (opening brace on the same line as the statement).
  ```cpp
  void function() {
    if (condition) {
      // do something
    } else {
      // do something else
    }
  }
  ```
- **Single-line Statements:** *Never* use short `if` statements, i.e. `if` statements that can be written on a single 
- line if it improves readability.
  ```cpp
  if (moveHorz) servoHorizontal.write(currentHorz);
  ```

## Naming Conventions
- **Namespaces:** PascalCase (e.g., `namespace Config`).
- **Classes:** PascalCase (e.g., `class PowerManager`).
- **Methods and Functions:** camelCase (e.g., `void setup()`, `void activateSensors()`).
- **Variables:** camelCase (e.g., `int currentHorz`).
- **Constants:** camelCase (e.g., `const byte servoHorizontalPin = 9;`).
- **Private Class Members:** camelCase with a leading underscore (e.g., `byte _sensorPin;`).

## Project Structure
- **Configuration:** All hardware pins and tunable parameters should be placed in the `Config` namespace.
- **Modularity:** Use classes to encapsulate functionality (e.g., `PowerManager`, `SolarServo`, `LightSensorArray`).
- **Logging:** Use the `Logger` class to collect information during a loop cycle and print it in a single structured line to minimize Serial overhead.

## Hardware Interaction
- Always use `Config::` constants for pin numbers.
- Ensure power-hungry components (servos, sensors) are deactivated when not in use.
- Include stabilization delays after powering up components before reading or moving.
