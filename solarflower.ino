/**
* Solar Flower
*
* Move towards the strongest light source and load the battery. The setup is optimized for minimum power consumption
*
* Author: Alexander Wendt
*/
#include <LowPower.h>   //Use low power between the cycles to save power
#include <Servo.h>
#include <math.h>

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
  const int servoHorizontalInitAngle = 135;  //set the initial angle to 90 degree, range [0, 135, 270]
  const int servoVerticalInitAngle = 90;    //set the initial angle to 90 degree range [0 (horizontal), 90 (vertical)]
  const byte m_speed = 20;    //set delay time to adjust the speed of servo;the longer the time, the smaller the speed
  const byte resolution = 1;  //set the rotation accuracy of the servo, the minimum rotation angle
  const bool activateMovement = true; //Activate movement

  // Photo resistors
  const byte initError = 5;      //Define the error range to prevent vibration
  const byte lowLightError = 60;  //Error to consider if the light is weak, else the system moves around withoout having any light
  const byte afterSleepError = 10; //Directly after sleep, use a higher tolerance value to prevent the system from moving just a little
  const int photoRightCalibration = -30;  //Balance sensors as they are not equally calibrated
  const int photoRightMinCalibration = -20;  //Balance the minimum calibration value, by setting the min value, which the calibration can do
  const int minPhotoResistorSolarValue = 730; //The min average value of the photoresistors to be able to generate power with the PV
  const byte measurementsPerCycle = 3;      //Number of measurements to average
  const int measurementInterval = 100;      //Interval between measurements in ms

  // Power
  const int initLoopDelay = 100;   //turn delay for the initialization
  const uint16_t shortSleepTime = 10;
  const uint16_t longSleepTime = 60;
  //const uint16_t longSleepTime = 5;
}

// ---------------------------------------------------------------------------
// Logger: collects tokens during a loop cycle, prints one structured line.
// Example output:
//   [SENSORS] U:712 D:698 L:543 R:501 avg:613 | [HORZ] 135deg LEFT | [VERT] 90deg STEADY | [PWR] SLEEP 60s #2
// ---------------------------------------------------------------------------
class Logger {
public:
  void clear() {
    _buf[0] = '\0';
    _len = 0;
  }

  void add(const String& token) {
    if (_len > 0) {
      append(" | ");
    }
    append(token.c_str());
  }

  void flush() {
    Serial.println(_buf);
    clear();
  }

private:
  char _buf[256];
  int  _len = 0;

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
  PowerManager(byte sensorPin, byte servoPin) : _sensorPin(sensorPin), _servoPin(servoPin) {}

  void setup() {
    pinMode(_sensorPin, OUTPUT);
    pinMode(_servoPin, OUTPUT);
    // Sensor pin (4): HIGH = ON, LOW = OFF
    // Servo  pin (5): LOW  = ON, HIGH = OFF
    off(); // Start in safe/off state
  }

  void activateSensors() {
    digitalWrite(_sensorPin, HIGH);
    delay(400); // Stabilization delay from original readPhotoSensors
  }

  void deactivateSensors() {
    digitalWrite(_sensorPin, LOW);
  }

  void activateServos() {
    digitalWrite(_servoPin, LOW);
    delay(200); // Power up delay
  }

  void deactivateServos() {
    digitalWrite(_servoPin, HIGH);
  }

  void off() {
    deactivateSensors();
    deactivateServos();
  }

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

  void idle(unsigned int ms) {
    delay(ms);
  }

private:
  byte _sensorPin;
  byte _servoPin;
};

class SolarServo {
public:
  SolarServo(byte pin, int minAngle, int maxAngle)
    : _pin(pin), _angle(0), _minAngle(minAngle), _maxAngle(maxAngle) {}

  void attach() {
    if (!_servo.attached()) {
      _servo.attach(_pin);
      // Note: We don't use standard attach(pin, min, max) because we map manually to preserve precise timing behavior
    }
  }

  void detach() {
    if (_servo.attached()) {
      _servo.detach();
    }
  }

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

  int getAngle() {
    return _angle;
  }
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

  LightSensorArray() {}

  void init() {
    pinMode(Config::photoDown, INPUT);
    pinMode(Config::photoLeft, INPUT);
    pinMode(Config::photoUp, INPUT);
    pinMode(Config::photoRight, INPUT);
  }

  const Readings& read(PowerManager& pm) {
    pm.activateSensors();

    long sumDown = 0;
    long sumLeft = 0;
    long sumUp = 0;
    long sumRight = 0;
    int iterAverages[Config::measurementsPerCycle];

    for (byte i = 0; i < Config::measurementsPerCycle; i++) {
      int d = analogRead(Config::photoDown);
      int l = analogRead(Config::photoLeft);
      int u = analogRead(Config::photoUp);
      int rawR = analogRead(Config::photoRight);
      int r = calibrateRightSensor(rawR);

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

  int adjustLeftSensor(int rawValue) {
    float minCalibrationValue = 0.0;
    float maxCalibrationValue = 1023.0;
    
    float a = 0.01;
    float b = 0.045;
    float c = 0.185;
    float tao1 = 90;
    float tao2 = 650;

    float calibrationValue = a + b * exp(-(float)rawValue/tao1) + c * exp(-(float)rawValue/tao2);
    float newSensorValue = (float)rawValue * (1 + calibrationValue);

    float correctedSensorValue = newSensorValue;
    if (newSensorValue > maxCalibrationValue) {
      correctedSensorValue = maxCalibrationValue;
    }
    if (newSensorValue < minCalibrationValue) {
      correctedSensorValue = minCalibrationValue;
    }

    return (int)correctedSensorValue;
  }

  /*int calibrateRightSensor(int rawValue) {
    // Extracted calibration logic
    float minCalibrationValue = 950.0;
    float maxCalibrationValue = 1023.0;
    int calibratedValue = 0;
    int calcCalibration = Config::photoRightCalibration;

    if (rawValue < minCalibrationValue) {
      calibratedValue = rawValue + Config::photoRightCalibration;
    } else if (rawValue > maxCalibrationValue) {
      calibratedValue = rawValue + Config::photoRightMinCalibration;
      calcCalibration = Config::photoRightMinCalibration; // for logging if needed
    } else {
      calcCalibration = (int)((Config::photoRightCalibration - Config::photoRightMinCalibration) *
                        ((maxCalibrationValue - (float)rawValue) / (maxCalibrationValue - minCalibrationValue)));
      calibratedValue = rawValue + calcCalibration + Config::photoRightMinCalibration;
    }
    return calibratedValue;
  }*/

  void log() {
    logger.add("[SENSORS] U:" + String(_readings.up)    +
               " D:"          + String(_readings.down)  +
               " L:"          + String(_readings.left)  +
               " R:"          + String(_readings.right) +
               " avg:"        + String(_readings.average) +
               " var:"        + String(_readings.variance) +
               " Vcc:"        + String((int)_readings.vcc_mV) + "mV");
  }

  const Readings& getValues() {
    return _readings;
  }

private:
  Readings _readings;

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
SolarServo servoHorizontal(Config::servoHorizontalPin, 0, 270);
SolarServo servoVertical(Config::servoVerticalPin, 0, 90);
LightSensorArray sensors;

// State variables
int longSleepCount = 1;
bool doSleep = false;
int sleepTime = Config::shortSleepTime;


void setup() {
  Serial.begin(9600);
  Serial.println("Start Solar Flower");

  powerManager.setup();
  sensors.init();

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

void updateLogic() {
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
  bool   moveHorz    = false;
  int    currentHorz = servoHorizontal.getAngle();
  String horzMsg;

  if (abs(val.left - val.right) <= error) {
    horzMsg = "STEADY";
  } else {
    if (val.left > val.right) {
      currentHorz -= Config::resolution;
      horzMsg = "LEFT";
    } else {
      currentHorz += Config::resolution;
      horzMsg = "RIGHT";
    }

    if (currentHorz <= 0) {
      currentHorz = 0;
      horzMsg = "LEFT(LIMIT)";
    } else if (currentHorz >= 270) {
      currentHorz = 270;
      horzMsg = "RIGHT(LIMIT)";
    } else {
      moveHorz = true;
    }
  }
  logger.add("[HORZ] " + String(currentHorz) + "deg " + horzMsg);

  // ---- Vertical ----
  bool   moveVert    = false;
  int    currentVert = servoVertical.getAngle();
  String vertMsg;

  if (abs(val.down - val.up) <= error) {
    vertMsg = "STEADY";
  } else {
    if (val.down < val.up) {
      currentVert -= Config::resolution;
      vertMsg = "UP";
    } else {
      currentVert += Config::resolution;
      vertMsg = "DOWN";
    }

    if (currentVert <= 0) {
      currentVert = 0;
      vertMsg = "UP(LIMIT)";
    } else if (currentVert >= 90) {
      currentVert = 90;
      vertMsg = "DOWN(LIMIT)";
    } else {
      moveVert = true;
    }
  }
  logger.add("[VERT] " + String(currentVert) + "deg " + vertMsg);

  // ---- Apply movements ----
  if (moveHorz || moveVert) {
    doSleep = false;
    powerManager.activateServos();
    if (moveHorz) {
      servoHorizontal.write(currentHorz);
    }
    if (moveVert) {
      servoVertical.write(currentVert);
    }
  } else {
    doSleep = true;
    powerManager.deactivateServos();
  }
}

bool checkErrors() {
  // Error check
  const auto& val = sensors.getValues();
  bool errorStateSensorsNoPower = (val.down == 0 && val.up == 0);
  if (errorStateSensorsNoPower) {
    logger.add("[ERROR] sensors U+D=0");
  }

  

  return errorStateSensorsNoPower;
}

void loop() {
  logger.clear();

  // Read sensors  →  adds [SENSORS] token
  sensors.read(powerManager);

  // Error check
  bool errorState = false;
  errorState = checkErrors();

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
