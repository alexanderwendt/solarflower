/**
* Solar Flower
*
* Move towards the strongest light source and load the battery. The setup is optimized for minimum power consumption
*
* Author: Alexander Wendt
*/
#include <LowPower.h>   //Use low power between the cycles to save power

//--- Constants for pins ---//
//const byte interruptButtonPin = 2;  //the pin of button;the corruption is disrupted
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

//const byte lightsensoraddress = 0x23;

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

// Power
const int initLoopDelay = 100;   //turn delay for the initialization
const uint16_t shortSleepTime = 10;
const uint16_t longSleepTime = 60;


//--- Global variables ---//
// Photo resistors
int photoDownValue = 0;
int photoLeftValue = 0;
int photoUpValue = 0;
int photoRightValue = 0;
int error = initError;
unsigned int turnDelay = initLoopDelay;
bool errorState = false;
unsigned int photoResistorAverageValue = 0;


// Servos
// Horizontal movement
bool doHorizontalMovement = true;
int servoHorizontalAngle = servoHorizontalInitAngle;
// Vertical movement
bool doVerticalMovement = true;
int servoVerticalAngle = servoVerticalInitAngle;

// Power
bool doSleep = false;
int sleepTime = shortSleepTime;
int longSleepCount = 1;

/**
* Setup method for Solar Flower
*/
void setup() {
  //Init digital IO as low to save power
  /*for (int i=0; i<20; i++) {
    //Set all to output except the analog photoread points
    if (i != powerDeactivationPin && i != servoHorizontalPin && i != servoVerticalPin) {
      pinMode(i, OUTPUT);
    }
  }*/

  Serial.begin(9600); //define the serial baud rate
  Serial.println("Start Solar Flower");

  pinMode(powerDeactivationPin, OUTPUT);  //Power deactivation pin photoresistors
  pinMode(servoPowerDeactivationPin, OUTPUT); //Servo power deactivation pin

  //Photo sensors
  pinMode(photoDown, INPUT); //set the mode of pin for phot resistors
  pinMode(photoLeft, INPUT);
  pinMode(photoUp, INPUT);
  pinMode(photoRight, INPUT);

  //Read sensors init
  readPhotoSensors();
  // Set actuators to init value
  controlActuators();

  // Set power on options
  digitalWrite(powerDeactivationPin, LOW); //Write high to activate devices
  digitalWrite(servoPowerDeactivationPin, HIGH); //Write high to activate devices
  delay(1000);
}

/**
* Main loop of the system
*/
void loop() {
  //Read photo sensors
  readPhotoSensors();

  checkErrorState();
  //Reason on how to proceed
  if (activateMovement==true && errorState==0) {
    reasonAboutNextSteps();
    //Servo action
    controlActuators();
  }
  //Show LCD values
  //Delay for activity
  handleSleep();

  Serial.println();
  delay(100); //Let serial printout complete
}

/**
* Check if the system is in an error state, i.e. if photo sensors are 0, where they should not be
*/
void checkErrorState() {
  if (photoDownValue == 0 && photoUpValue == 0){  //Photo resistors are disconnected
    errorState = true;
    Serial.println("Errorstate. Photo resistors up and down are 0.");
  } else {
    errorState = false;
  }
}

/**
* Read all phot resistor values
*/
void readPhotoSensors() {
  digitalWrite(powerDeactivationPin, HIGH); //Write high to activate devices
  delay(200); //Delay necessary to stabilize the sensor read signal on power on
  photoDownValue = analogRead(photoDown);
  photoLeftValue = analogRead(photoLeft);
  photoUpValue = analogRead(photoUp);
  int photoRightValueUncalibrated = analogRead(photoRight);
  int photoRightCalibrationCalculated = photoRightCalibration;
  //For higher values, the calibration value does not apply, reduce
  //float photoRightCalibrationCalculated2 = ();
  //Serial.print("cal: " + String(photoRightCalibrationCalculated2) + ";t:" + String(photoRightValueUncalibrated));
  
  float minCalibrationValue = 950.;
  float maxCalibrationValue = 1023.;
  if (photoRightValueUncalibrated<minCalibrationValue) {
    photoRightValue = photoRightValueUncalibrated + photoRightCalibration;
  } else if (photoRightValueUncalibrated>maxCalibrationValue) {
    photoRightCalibrationCalculated = photoRightMinCalibration;
    photoRightValue = photoRightValueUncalibrated + photoRightCalibration;
  } else {
    photoRightCalibrationCalculated = (int)((photoRightCalibration-photoRightMinCalibration)*((maxCalibrationValue-(float)photoRightValueUncalibrated)/(maxCalibrationValue-minCalibrationValue)));
    photoRightValue = photoRightValueUncalibrated + photoRightCalibrationCalculated + photoRightMinCalibration;
  }

  photoResistorAverageValue = (int)(photoDownValue + photoLeftValue + photoUpValue + photoRightValue)/4;

  Serial.print("PUp: " + String(photoUpValue) + ", PDown: " + String(photoDownValue) + "| PLeft: " + 
    String(photoLeftValue) + ", PRight: " + String(photoRightValue) + " (" + String(photoRightCalibrationCalculated) + ")| Average: " + String(photoResistorAverageValue) + "|");
}

/**
* Based on sensor values, reason about the next servo movements left
*/
void reasonAboutNextSteps() {
  //At low light, use a higher error to prevent the system from moving all the time
  if (photoResistorAverageValue >= minPhotoResistorSolarValue) {
    if (longSleepCount>=1) {  //If previously woke up from sleep, set a larger error as buffer to prevent unnecessary movements
      error = afterSleepError;
    } else {
      error = initError;
    }
    
    sleepTime = shortSleepTime;
  } else {
    if (longSleepCount>=1) {  //If previously woke up from sleep, set a larger error as buffer to prevent unnecessary movements
      error = lowLightError;
    } else {
      error = initError;
    }

    sleepTime = longSleepTime;
  }

  //If long sleep is performed 2x in a row, not much happends and the long delay can be used 
  if (longSleepCount >= 2) {
    sleepTime = longSleepTime;
  } else {
    sleepTime = shortSleepTime;
  }

  if (abs(photoLeftValue - photoRightValue) <= error) { //Within error. Do nothing
    Serial.print("Hor angle: " + String(servoHorizontalAngle) + ". Hor diff < " + String(error) + ". ");
    doHorizontalMovement = false;
  } else {  //Movement allowed
    if(photoLeftValue > photoRightValue) {
      //left is brighter than right, move left, move to the light
      servoHorizontalAngle -= resolution;
      Serial.print("Hor angle: " + String(servoHorizontalAngle) + ". WANT TURN LEFT. ");
    } else if (photoLeftValue < photoRightValue) {
      servoHorizontalAngle += resolution;
      Serial.print("Hor angle: " + String(servoHorizontalAngle) + ". WANT TURN RIGHT. ");
    }

    if (servoHorizontalAngle <= 0) {   
      servoHorizontalAngle = 0;
      doHorizontalMovement = false;
      Serial.print("servo hor angle = 0. NO TURN LEFT.");
    } else if (servoHorizontalAngle >= 270) { //limit the rotation angle of servo
      servoHorizontalAngle = 270;
      doHorizontalMovement = false;
      Serial.print("servo hor angle = 270, NO TURN RIGHT. ");
    } else {
      doHorizontalMovement = true;
    }
  }

  //Vertical movement
  if (abs(photoDownValue - photoUpValue) <= error) {
    Serial.print("Vert angle: " + String(servoVerticalAngle) + ". Vert diff < " + String(error) + ". ");
    doVerticalMovement = false;
  } else {
    if(photoDownValue < photoUpValue) {
    //left is brighter than right, move left, move to the light
      servoVerticalAngle -= resolution;
      Serial.print("Vert angle: " + String(servoVerticalAngle) + ". WANT TURN UP. ");
    } else if (photoDownValue > photoUpValue) {
      servoVerticalAngle += resolution;
      Serial.print("Vert angle: " + String(servoVerticalAngle) + ". WANT TURN DOWN. ");
    }

    if (servoVerticalAngle <= 0) {
      servoVerticalAngle = 0;
      doVerticalMovement = false;
      Serial.print("Vert angle = 0. NO TURN UP. ");
    } else if (servoVerticalAngle >= 90) { //limit the rotation angle of servo for top to max 135 degree
      servoVerticalAngle = 90;
      doVerticalMovement = false;
      Serial.print("Vert angle = 90. NO TURN DOWN. ");
    } else {
      doVerticalMovement = true;
    }
  }

  //If any change proposal has been done for both horizontal and vertical servo, set the turn delay to init value
  if (doHorizontalMovement==1 || doVerticalMovement==1) {
    doSleep = false;
  } else {
    doSleep = true;
  }
}

/**
* Attach controllers and set the control positons
*/
void controlActuators() {
  if (doHorizontalMovement==1 || doVerticalMovement==1) {
    digitalWrite(servoPowerDeactivationPin, LOW);
    delay(200); //Let system get powered before setting the commands
  } else {
    digitalWrite(servoPowerDeactivationPin, HIGH);
  }

  if (doHorizontalMovement==1) {
    setServoHorizontalAngle(servoHorizontalAngle);
  } 
  
  if (doVerticalMovement==1) {
    setServoVerticalAngle(servoVerticalAngle);
  }  
}

/**
* Sleep and deep sleep through power down
*/
void handleSleep() {
  if (doSleep==1) {
    Serial.print("Sleep " + String(sleepTime));
    digitalWrite(powerDeactivationPin, LOW);  //Write to deactivate all devices set by the transistor
    digitalWrite(servoPowerDeactivationPin, HIGH);

    longSleepCount = longSleepCount + 1;  //Increment the long sleep count
    delay(500); //Else TX led is on
    longSleep(sleepTime);
  } else {
    //Use turn delay
    Serial.print("delay " + String(turnDelay));
    longSleepCount = 0;   //Longsleep cancelled, set to 0
    delay(turnDelay);
  }
}

/**
* Sleep with power down for a number of seconds, handling sleep times > 8s
*
**/
void longSleep(uint16_t sleepInSeconds) {   
  if ( sleepInSeconds & 0x01 ) LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
  if ( sleepInSeconds & 0x02 ) LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  if ( sleepInSeconds & 0x04 ) LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);

  while ( sleepInSeconds & 0xFFF8 ) {
    sleepInSeconds = sleepInSeconds - 8;
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}

/**
* Set servo angle for the horizontal servo
*
**/
void setServoHorizontalAngle(int myAngle) { //the function of pluse
  int minAngle = 0;
  int maxAngle = 270;
  int pulsewidth = map(myAngle, minAngle, maxAngle, 500, 2500); //Map angle to pulse width e.g. 0, 180
  if (myAngle >= minAngle && myAngle <= maxAngle) {
    for (int i = 0; i < 10; i++) { //output pulse
      digitalWrite(servoHorizontalPin, HIGH);//set the servo interface level to high
      delayMicroseconds(pulsewidth);//the delay time of pulse width
      digitalWrite(servoHorizontalPin, LOW);//turn the servo interface level to low
      delay(20 - pulsewidth / 1000);
    }
  }
}

/**
* Set servo angle for the vertical servo
*
**/
void setServoVerticalAngle(int myAngle) { //the function of pluse
  int minAngle = 0;
  int maxAngle = 180;
  int pulsewidth = map(myAngle, minAngle, maxAngle, 500, 2500); //Map angle to pulse width e.g. 0, 180
  if (myAngle >= minAngle && myAngle <= maxAngle) {
    for (int i = 0; i < 10; i++) { //output pulse
      digitalWrite(servoVerticalPin, HIGH);//set the servo interface level to high
      delayMicroseconds(pulsewidth);//the delay time of pulse width
      digitalWrite(servoVerticalPin, LOW);//turn the servo interface level to low
      delay(20 - pulsewidth / 1000);
    }
  }
}
