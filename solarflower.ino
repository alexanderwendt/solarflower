/**
* Solar Flower
*
* Move towards the strongest light source and load the battery. The setup is optimized for minimum power consumption
*
* Author: Alexander Wendt
*/
//#define DEBUG

#include <LowPower.h>   //Use low power between the cycles to save power
#include <Servo.h>
#include <math.h>
#include <avr/wdt.h>

namespace Config {
  //--- Constants for pins ---//
  // Digital pins
  const byte servoHorizontalPin = 9;
  const byte servoVerticalPin = 10;
  const byte powerDeactivationPin = 4;  // Power off sensors, i.e. photo resistors
  const byte servoPowerDeactivationPin = 5; // Power off servos

  // Analogue pins
  const int photoDown = A0;
  const int photoLeft = A1;
  const int photoUp = A2;
  const int photoRight = A3;

  //--- Constants for initialization ---//
  // Servos
  const int servoHorizontalMinAngle = 0;
  const int servoHorizontalMaxAngle = 270;
  const int servoVerticalMinAngle = 0;
  const int servoVerticalMaxAngle = 90;
  const int servoHorizontalInitAngle = 135;  //set the initial angle to 90 degree, range [0, 135, 270]
  const int servoVerticalInitAngle = 45;    //set the initial angle to 90 degree range [0 (horizontal), 90 (vertical)]
  const int minVerticalPriorityDegree = 40; //Below this angle, horizontal movement must be finished before vertical movement starts
  const byte m_speed = 20;    //set delay time to adjust the speed of servo;the longer the time, the smaller the speed
  const byte resolution = 1;  //set the rotation accuracy of the servo, the minimum rotation angle
  const bool activateMovement = true; //Activate movement

  // Photo resistors
  const byte initError = 5;      //Define the error range to prevent vibration
  const byte lowLightError = 60;  //Error to consider if the light is weak, else the system moves around withoout having any light
  const byte afterSleepError = 10; //Directly after sleep, use a higher tolerance value to prevent the system from moving just a little
  const int minPhotoResistorSolarValue = 430; //The min average value of the photoresistors to be able to generate power with the PV
  const byte measurementsPerCycle = 3;      //Number of measurements to average
  const int measurementInterval = 100;      //Interval between measurements in ms

  // Power
  const int initLoopDelay = 100;   //turn delay for the initialization
  #ifdef DEBUG
    const uint16_t shortSleepTime = 3;  //time to sleep while short after a movement
    const uint16_t longSleepTime = 2;
    const int maxLongSleepCount = 2;
    const int lowPowerSleeptime = 3;
  #else
    const uint16_t shortSleepTime = 30;  //time to sleep while short after a movement. 30s to prevent moving all the time as the sun moves
    const uint16_t longSleepTime = 60;
    const int maxLongSleepCount = 60;  //Sleep count before slow reset is activated
    const int lowPowerSleeptime = 86400; //If low power, the battery must be charged first, sleep 24h
  #endif

  const float minInternalVoltage = 4.7;  //If the internal voltage drops below 3.0V, the battery is almost empty and there is not enough power to drive the servos
}

// ---------------------------------------------------------------------------
// Logger: collects tokens during a loop cycle, prints one structured line.
// Example output:
//   [SENSORS] U:712 D:698 L:543 R:501 avg:613 | [HORZ] 135deg LEFT | [VERT] 90deg STEADY | [PWR] SLEEP 60s #2
// ---------------------------------------------------------------------------
class Logger {
public:
  // Clears the current log line so a new loop cycle can collect fresh tokens.
  void clear() {
    _buf[0] = '\0';
    _len = 0;
  }

  // Adds one token to the current log line, separated from previous tokens.
  void add(const String& token) {
    if (_len > 0) {
      append(" | ");
    }
    append(token.c_str());
  }

  // Prints the collected log line and clears the buffer afterwards.
  void flush() {
    Serial.println(_buf);
    clear();
  }

private:
  char _buf[256];
  int  _len = 0;

  // Appends text to the internal buffer while preventing buffer overflow.
  void append(const char* s) {
    while (*s && _len < (int)sizeof(_buf) - 1) {
      _buf[_len++] = *s++;
    }
    _buf[_len] = '\0';
  }
};

Logger logger;

// ---------------------------------------------------------------------------

class PowerManager {
public:
  // Stores the pins used to switch sensor and servo power.
  PowerManager(byte sensorPin, byte servoPin) : _sensorPin(sensorPin), _servoPin(servoPin) {}

  // Configures power control pins and starts with sensors and servos switched off.
  void setup() {
    pinMode(_sensorPin, OUTPUT);
    pinMode(_servoPin, OUTPUT);
    // Sensor pin (4): HIGH = ON, LOW = OFF
    // Servo  pin (5): LOW  = ON, HIGH = OFF
    off(); // Start in safe/off state
  }

  // Switches the sensor power on and waits until readings are stable.
  void activateSensors() {
    digitalWrite(_sensorPin, HIGH);
    delay(400); // Stabilization delay from original readPhotoSensors
  }

  // Switches the sensor power off to reduce current draw.
  void deactivateSensors() {
    digitalWrite(_sensorPin, LOW);
  }

  // Switches the servo power on and waits until the supply is ready.
  void activateServos() {
    digitalWrite(_servoPin, LOW);
    delay(200); // Power up delay
  }

  // Switches the servo power off to reduce current draw.
  void deactivateServos() {
    digitalWrite(_servoPin, HIGH);
  }

  // Switches all controlled loads off.
  void off() {
    deactivateSensors();
    deactivateServos();
  }

  // Sleeps for the requested duration by chaining watchdog sleep intervals.
  void sleep(uint16_t seconds) {
    off(); // Ensure everything is off

    // Handle splits for long sleep times
    while (seconds >= 8) {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
      seconds -= 8;
    }
    if (seconds >= 4) {
      LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
      seconds -= 4;
    }
    if (seconds >= 2) {
      LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
      seconds -= 2;
    }
    if (seconds >= 1) {
      LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
      seconds -= 1;
    }
  }

  // Performs a reset of the whole system using the watchdog timer.
  void performReset() {
    wdt_enable(WDTO_15MS);  // enable watchdog with short timeout
    while (true) {}         // wait for reset
  }

  // Keeps the controller awake for a short active delay.
  void idle(unsigned int ms) {
    delay(ms);
  }

private:
  byte _sensorPin;
  byte _servoPin;
};

class SolarServo {
public:
  // Stores servo pin and angle limits for later movement commands.
  SolarServo(byte pin, int minAngle, int maxAngle)
    : _pin(pin), _angle(0), _minAngle(minAngle), _maxAngle(maxAngle) {}

  // Attaches the servo output if it is not attached yet.
  void attach() {
    if (!_servo.attached()) {
      _servo.attach(_pin);
      // Note: We don't use standard attach(pin, min, max) because we map manually to preserve precise timing behavior
    }
  }

  // Detaches the servo output so the pin stops generating pulses.
  void detach() {
    if (_servo.attached()) {
      _servo.detach();
    }
  }

  // Moves the servo to a constrained logical angle using calibrated pulse widths.
  void write(int angle) {
    _angle = constrain(angle, _minAngle, _maxAngle);

    // Map logical angle range to 500-2500µs pulse width
    int pulsewidth = (_maxAngle == 270)
      ? map(_angle, 0, 270, 500, 2500)
      : map(_angle, 0, 180, 500, 2500);

    if (!_servo.attached()) {
      attach();
    }
    _servo.writeMicroseconds(pulsewidth);
    delay(200); // Allow servo time to reach position
  }

  // Returns the last commanded logical angle.
  int getAngle() {
    return _angle;
  }

  // Updates the stored logical angle without physically moving the servo.
  void setAngleNoMove(int angle) {
    _angle = angle;
  }

private:
  Servo _servo;
  byte _pin;
  int _angle;
  int _minAngle;
  int _maxAngle;
};

class LightSensorArray {
public:
  struct Readings {
    int down;
    int left;
    int up;
    int right;
    int average;
    float variance;
    long vcc_mV;
  };

  // Creates the sensor array wrapper; hardware setup is done separately in init().
  LightSensorArray() {}

  // Configures all photo resistor pins as inputs.
  void init() {
    pinMode(Config::photoDown, INPUT);
    pinMode(Config::photoLeft, INPUT);
    pinMode(Config::photoUp, INPUT);
    pinMode(Config::photoRight, INPUT);
  }

  // Powers sensors, reads and averages photo resistor values, measures Vcc, and logs the result.
  const Readings& read(PowerManager& pm) {
    pm.activateSensors();

    long sumDown = 0;
    long sumLeft = 0;
    long sumUp = 0;
    long sumRight = 0;
    int iterAverages[Config::measurementsPerCycle];

    for (byte i = 0; i < Config::measurementsPerCycle; i++) {
      int d = 1023 - analogRead(Config::photoDown); //Inverted photoresistors, 0 = very bright, 1023 = dark
      int rawL = 1023 - analogRead(Config::photoLeft);
      int l = rawL;
      int u = 1023 - analogRead(Config::photoUp);
      int rawR = 1023 - analogRead(Config::photoRight);
      int r = rawR;

      sumDown += d;
      sumLeft += l;
      sumUp += u;
      sumRight += r;

      iterAverages[i] = (d + l + u + r) / 4;

      if (i < (Config::measurementsPerCycle - 1)) {
        delay(Config::measurementInterval);
      }
    }

    _readings.down = (int)(sumDown / Config::measurementsPerCycle);
    _readings.left = (int)(sumLeft / Config::measurementsPerCycle);
    _readings.up = (int)(sumUp / Config::measurementsPerCycle);
    _readings.right = (int)(sumRight / Config::measurementsPerCycle);

    _readings.average = (_readings.down + _readings.left + _readings.up + _readings.right) / 4;

    // Calculate variance of the cycle averages
    float meanIter = 0;
    for (byte i = 0; i < Config::measurementsPerCycle; i++) {
      meanIter += (float)iterAverages[i];
    }
    meanIter /= Config::measurementsPerCycle;

    float varSum = 0;
    for (byte i = 0; i < Config::measurementsPerCycle; i++) {
      float diff = (float)iterAverages[i] - meanIter;
      varSum += diff * diff;
    }
    _readings.variance = varSum / Config::measurementsPerCycle;

    _readings.vcc_mV = readVcc_mV();

    log();
    return _readings;
  }

  // Adds the latest sensor values to the structured loop log.
  void log() {
    logger.add("[SENSORS] U:" + String(_readings.up)    +
               " D:"          + String(_readings.down)  +
               " L:"          + String(_readings.left)  +
               " R:"          + String(_readings.right) +
               " avg:"        + String(_readings.average) +
               " var:"        + String(_readings.variance) +
               " Vcc:"        + String((int)_readings.vcc_mV) + "mV");
  }

  // Returns the latest cached sensor and voltage readings.
  const Readings& getValues() {
    return _readings;
  }

  // Measures and returns the current live supply voltage without using cached readings.
  long measureVcc_mV() {
    return readVcc_mV();
  }

private:
  Readings _readings;

  // Measures supply voltage in millivolts using the internal 1.1V reference.
  long readVcc_mV() {
    uint8_t admux_old = ADMUX;

    // For ATmega328P (Nano/Uno)
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1); // Select VBG 1.1V, reference = VCC

    delayMicroseconds(200);
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
      // Wait for conversion
    }
    uint16_t adc = ADC;

    ADMUX = admux_old;

    const long VBG_mV = 1100;  // Adjust if you calibrate it
    return (VBG_mV * 1023L) / (adc ? adc : 1);
  }
};

//--- Global Objects ---//
PowerManager powerManager(Config::powerDeactivationPin, Config::servoPowerDeactivationPin);
SolarServo servoHorizontal(Config::servoHorizontalPin, Config::servoHorizontalMinAngle, Config::servoHorizontalMaxAngle);
SolarServo servoVertical(Config::servoVerticalPin, Config::servoVerticalMinAngle, Config::servoVerticalMaxAngle);
LightSensorArray sensors;

// State variables
int longSleepCount = 1;
bool doSleep = false;
int sleepTime = Config::shortSleepTime;
bool slowReset = false;

void applyLowPowerSleep(const String& reason, long vcc_mV);
bool hasLowPowerError();
bool hasLowPowerError(long& vcc_mV);


// Initializes serial output, power control, sensors, and the servo start position.
void setup() {
  Serial.begin(9600);
  Serial.println("Start Solar Flower");

  #ifdef DEBUG
  Serial.println("Debug mode");
  #endif

  powerManager.setup();
  sensors.init();
  
  delay(2000); // Let supply voltage stabilize after power-on

  // Initial read
  sensors.read(powerManager);

  // Initialize servos
  servoHorizontal.setAngleNoMove(Config::servoHorizontalInitAngle);
  servoVertical.setAngleNoMove(Config::servoVerticalInitAngle);

  // Power servos temporarily to set initial position
  powerManager.activateServos();
  servoHorizontal.write(Config::servoHorizontalInitAngle);
  servoVertical.write(Config::servoVerticalInitAngle);
  powerManager.deactivateServos();

  delay(1000);
}

// Moves both servo targets step-by-step back to their configured initial angles.
void handleSlowReset(int& currentHorz, int& currentVert, bool& moveHorz, bool& moveVert, String& horzMsg, String& vertMsg) {
  if (currentHorz != Config::servoHorizontalInitAngle) {
    if (currentHorz < Config::servoHorizontalInitAngle) {
      currentHorz += Config::resolution;
    } else {
      currentHorz -= Config::resolution;
    }
    moveHorz = true;
    horzMsg = "RESET";
  } else {
    horzMsg = "STEADY(INIT)";
  }

  if (currentVert != Config::servoVerticalInitAngle) {
    if (currentVert < Config::servoVerticalInitAngle) {
      currentVert += Config::resolution;
    } else {
      currentVert -= Config::resolution;
    }
    moveVert = true;
    vertMsg = "RESET";
  } else {
    vertMsg = "STEADY(INIT)";
  }

  if (!moveHorz && !moveVert) {
    slowReset = false;
    logger.add("[RESET] Finished");
  }
  sleepTime = Config::shortSleepTime;
}

// Calculates normal tracking movement from the latest light sensor readings.
void handleNormalMovement(int& currentHorz, int& currentVert, bool& moveHorz, bool& moveVert, String& horzMsg, String& vertMsg) {
  const auto& val = sensors.getValues();
  int error;

  // Determine error threshold and sleep time
  if (val.average >= Config::minPhotoResistorSolarValue) {
    error     = (longSleepCount >= 1) ? Config::afterSleepError : Config::initError;
    sleepTime = Config::shortSleepTime;
  } else {
    error     = (longSleepCount >= 1) ? Config::lowLightError : Config::initError;
    sleepTime = Config::longSleepTime;
  }

  if (longSleepCount >= 2) {
    sleepTime = Config::longSleepTime;
  } else {
    sleepTime = Config::shortSleepTime;
  }

  // ---- Horizontal ----
  bool horizontalSteady = (abs(val.left - val.right) <= error);
  if (horizontalSteady) {
    horzMsg = "STEADY";
  } else {
    if (val.left > val.right) {
      currentHorz -= Config::resolution;
      horzMsg = "LEFT";
    } else {
      currentHorz += Config::resolution;
      horzMsg = "RIGHT";
    }

    if (currentHorz < Config::servoHorizontalMinAngle) {
      currentHorz = Config::servoHorizontalMinAngle;
      horzMsg = "LEFT(LIMIT)";
    } else if (currentHorz > Config::servoHorizontalMaxAngle) {
      currentHorz = Config::servoHorizontalMaxAngle;
      horzMsg = "RIGHT(LIMIT)";
    } else {
      moveHorz = true;
    }
  }

  // ---- Vertical Priority Logic ----
  // If vertical angle < 40, vertical waits only while horizontal will actually move.
  // Between 90 and 40, both can move.

  bool doMoveDown = abs(val.down - val.up) > error && val.down > val.up;
  bool verticalAllowed = (currentVert >= Config::minVerticalPriorityDegree) || !moveHorz || doMoveDown;

  // ---- Vertical ----
  if (!verticalAllowed) {
    vertMsg = "WAITING(HORZ)";
  } else if (abs(val.down - val.up) <= error) {
    vertMsg = "STEADY";
  } else {
    if (val.down < val.up) {
      currentVert -= Config::resolution;
      vertMsg = "UP";
    } else {
      currentVert += Config::resolution;
      vertMsg = "DOWN";
    }

    if (currentVert < Config::servoVerticalMinAngle) {
      currentVert = Config::servoVerticalMinAngle;
      vertMsg = "UP(LIMIT)";
    } else if (currentVert > Config::servoVerticalMaxAngle) {
      currentVert = Config::servoVerticalMaxAngle;
      vertMsg = "DOWN(LIMIT)";
    } else {
      moveVert = true;
    }
  }
}

// Chooses reset or normal movement, applies servo moves, and updates sleep state.
void updateLogic() {
  int newHorz = servoHorizontal.getAngle();
  int newVert = servoVertical.getAngle();
  bool moveHorz = false;
  bool moveVert = false;
  String horzMsg;
  String vertMsg;

  if (slowReset) {
    handleSlowReset(newHorz, newVert, moveHorz, moveVert, horzMsg, vertMsg);
  } else {
    handleNormalMovement(newHorz, newVert, moveHorz, moveVert, horzMsg, vertMsg);
  }

  #ifdef DEBUG
    Serial.println("[DEBUG] slowreset: " + String(slowReset) + ". Pos: H=" + String(servoHorizontal.getAngle()) + "deg, V=" + String(servoVertical.getAngle()) + "deg");
  #endif

  logger.add("[GO HOR] " + String(newHorz) + "deg " + horzMsg);
  logger.add("[GO VER] " + String(newVert) + "deg " + vertMsg);

  // ---- Apply movements ----
  if (moveHorz || moveVert) {
    doSleep = false;
    powerManager.activateServos();
    if (moveHorz) {
      servoHorizontal.write(newHorz);
    }
    if (moveVert) {
      servoVertical.write(newVert);
    }

    long vcc_mV;
    if (hasLowPowerError(vcc_mV)) {  // Check if there is a power drop after activation of the servos
      powerManager.deactivateServos();
      applyLowPowerSleep("low voltage under servo load", vcc_mV);
      return;
    }

  } else {
    doSleep = true;
    powerManager.deactivateServos();
  }
}


// Enables slow reset after many long sleeps when the vertical servo is at a limit.
void checkSlowReset() {
  int currentVert = servoVertical.getAngle();
  if (longSleepCount >= Config::maxLongSleepCount &&
      (currentVert == Config::servoVerticalMinAngle || currentVert == Config::servoVerticalMaxAngle)) {
    slowReset = true;
    logger.add("[RESET] Triggered after " + String(longSleepCount) + " sleeps. Pos: H=" + String(servoHorizontal.getAngle()) + "deg, V=" + String(currentVert) + "deg");
  }
}


// Applies low-power sleep settings and logs the measured voltage.
void applyLowPowerSleep(const String& reason, long vcc_mV) {
  doSleep = true;
  sleepTime = Config::lowPowerSleeptime;  //Very long sleep time to regenerate
  logger.add("[ERROR] " + reason + ": " + String((int)vcc_mV) + "mV Sleep time: " + String(sleepTime));
}

// Returns true when live voltage is too low and exposes the measured value.
bool hasLowPowerError(long& vcc_mV) {
  vcc_mV = sensors.measureVcc_mV();
  return (vcc_mV < (long)(Config::minInternalVoltage * 1000));
}

// Returns true when the measured supply voltage is below the configured minimum.
bool hasLowPowerError() {
  long vcc_mV;
  return hasLowPowerError(vcc_mV);
}

// Checks sensor and low-power errors and adjusts sleep behavior when needed.
bool checkErrors() {
  const auto& val = sensors.getValues();
  bool sensorError = (val.down == 1023 && val.up == 1023);
  long vcc_mV;
  bool lowPowerError = hasLowPowerError(vcc_mV);

  // Slow reset overrules sensor error but not low power
  bool errorState = (sensorError && !slowReset) || lowPowerError;

  if (errorState) {
    doSleep = true;
    sleepTime = Config::longSleepTime;

    if (sensorError && !slowReset) {
      logger.add("[ERROR] sensors U+D=0");
    }
    if (lowPowerError) {
      applyLowPowerSleep("low voltage", vcc_mV);
    }
  }
  return errorState;
}

// Runs one control cycle: read sensors, handle errors, move if needed, then sleep or idle.
void loop() {
  logger.clear();

  // Read sensors  →  adds [SENSORS] token
  sensors.read(powerManager);

  // Check if the system is in a bad state after 1h and then do reset to normal, but slow
  checkSlowReset();

  bool errorState = checkErrors();

  // Reason & act  →  adds [HORZ] and [VERT] tokens
  if (Config::activateMovement && !errorState) {
    updateLogic();
  }

  // Sleep / idle  →  adds [PWR] token, then flushes the complete line
  if (doSleep) {
    logger.add("[PWR] SLEEP " + String(sleepTime) + "s #" + String(longSleepCount));
    longSleepCount++;
    logger.flush();
    delay(100); // Let serial finish before sleep
    powerManager.sleep(sleepTime);
  } else {
    logger.add("[PWR] ACTIVE " + String(Config::initLoopDelay) + "ms");
    longSleepCount = 0;
    logger.flush();
    powerManager.idle(Config::initLoopDelay);
  }
}
