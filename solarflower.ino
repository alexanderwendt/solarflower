//Drivers
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  //Use panel
LiquidCrystal_I2C lcd(0x27, 16, 2);   //Panel

#include <BH1750.h>     //Light intensity meter
BH1750 lightMeter;

#include <Servo.h>
Servo servoHorizontal;  //define the name of the servo rotating right and left
Servo servoVertical;    //efine the name of the servo rotating upwards and downwards

#include <LowPower.h>   //Use low power between the cycles to save power

//Constants for pins
const byte interruptButtonPin = 2;  //the pin of button;the corruption is disrupted
const byte servoHorizontalPin = 9;
const byte servoVerticalPin = 10;
const byte ledLight = 3;
const byte buzzer = 6;
const int photoBack = A0;
const int photoLeft = A1;
const int photoFront = A2;
const int photoRight = A3;

//Constants for initialization
const int servoHorizontalInitAngle = 90;  //set the initial angle to 90 degree
const int servoVerticalInitAngle = 45;    //set the initial angle to 90 degree
const byte initError = 5;      //Define the error range to prevent vibration
const byte lowLightError = 30;  //Error to consider if the light is weak, else the system moves around withoout having any light
const int minLightSensorValue = 100;  //Min light to actively search for light source
const byte m_speed = 20;    //set delay time to adjust the speed of servo;the longer the time, the smaller the speed
const byte resolution = 1;  //set the rotation accuracy of the servo, the minimum rotation angle
const int initTurnDelay = 100;   //turn delay for the initialization
const int maxLightTurnDelay = 10000; //If a balance between the sensors could be found, sleep longer so save power
const bool activateMovement = true; //Activate movement
const int photoRightCalibration = -30;  //Balance sensors as they are not equally calibrated
const period_t sleepTime = SLEEP_8S;

//Global variables
int photoBottomValue = 0;
int photoLeftValue = 0;
int photoTopValue = 0;
int photoRightValue = 0;
byte error = initError;
unsigned int turnDelay = initTurnDelay;
bool activateLcd = false;

unsigned int lightSensorValue;   //save the variable of light intensity 

int servoHorizontalAngle = servoHorizontalInitAngle;
int servoVerticalAngle = servoVerticalInitAngle;
bool doSleep = false;

void setup() {
  Serial.begin(9600); //define the serial baud rate
  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin();
  lightMeter.begin();
  
  pinMode(interruptButtonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptButtonPin), controlLcd, FALLING); //xternal interrupt touch type is falling edge; adjust_resolution is interrupt service function ISR

  pinMode(ledLight, OUTPUT);
  pinMode(buzzer, OUTPUT);

  //Set to init position
  servoHorizontal.write(servoHorizontalInitAngle); //return to initial angle
  delay(100);
  servoVertical.write(servoVerticalInitAngle); //return to initial angle
  delay(100);
  //pinMode(servoHorizontalPin, OUTPUT);//set the pin of the servo
  //pinMode(servoVerticalPin, OUTPUT);//set the pin of the servo
  servoHorizontal.attach(servoHorizontalPin);  //link the servo to digital port 9
  servoVertical.attach(servoVerticalPin);  //link the servo to digital port 10

  //Photo sensors
  pinMode(photoBack, INPUT); //set the mode of pin for phot resistors
  pinMode(photoLeft, INPUT);
  pinMode(photoFront, INPUT);
  pinMode(photoRight, INPUT);

  //LCD init
  lcd.init();          // initialize the LCD
  lcd.noBacklight();     //set LCD backlight

  //Read sensors init
  readPhotoSensors();
  readLightIntensity();
}

void loop() {
  //Read photo sensors
  readPhotoSensors();
  //Read light intensity of PV
  readLightIntensity();
  //Reason on how to proceed
  if (activateMovement==true) {
    reasonAboutNextSteps();
    //Servo action
    controlActuators();
  }
  //Show LCD values
  showLcdValues();
  //Delay for activity
  handleSleep();

  Serial.println();
  

}

void readPhotoSensors() {
  photoBottomValue = analogRead(photoBack);
  photoLeftValue = analogRead(photoLeft);
  photoTopValue = analogRead(photoFront);
  photoRightValue = analogRead(photoRight);

  Serial.print("PhotoBack: " + String(photoBottomValue) + ", PhotoFront: " + String(photoTopValue) + ", PhotoLeft: " + 
    String(photoLeftValue) + ", PhotoRight: " + String(photoRightValue) + "(" + String(photoRightCalibration) + "). ");
}

void readLightIntensity() {
  lightSensorValue = lightMeter.readLightLevel();
  Serial.print("Light: " + String(lightSensorValue) + " lux. ");
}

void reasonAboutNextSteps() {
  //Steps
  //1. Check if movement necessary
  //2. Calculate horizontal angle for the next step 
  //3. Check if movement is allowed

  //Horizontal movement
  int photoRightValueCal = photoRightValue + photoRightCalibration;
  bool doHorizontalMovement = true;
  bool doVerticalMovement = true;

  //At low light, use a higher error to prevent the system from moving all the time
  if (lightSensorValue >= minLightSensorValue) {
    error = initError;
  } else {
    error = lowLightError;
  }

  if (abs(photoLeftValue - photoRightValueCal) <= error) {
    Serial.print("Hor light difference within limit: " + String(error) + ". Do nothing. ");
    doHorizontalMovement = false;
  } else {
    if(photoLeftValue > photoRightValueCal) {
    //left is brighter than right, move left, move to the light
      servoHorizontalAngle -= resolution;
      Serial.print("Servo hor angle: " + String(servoHorizontalAngle) + ", turn left. ");
    } else if (photoLeftValue < photoRightValueCal) {
      servoHorizontalAngle += resolution;
      Serial.print("Servo hor angle: " + String(servoHorizontalAngle) + ", turn right. ");
    }

    //write to horizontal servo
    if (servoHorizontalAngle <= 0) {
      servoHorizontalAngle = 0;
      doHorizontalMovement = false;
      Serial.print("servo horizontal angle = 0, do nothing. ");
    } else if (servoHorizontalAngle >= 180) { //limit the rotation angle of servo
      servoHorizontalAngle = 180;
      doHorizontalMovement = false;
      Serial.print("servo horizontal angle = 180, do nothing. ");
    } else {
      doHorizontalMovement = true;
      servoHorizontal.write(servoHorizontalAngle);
      delay(m_speed);
    } 
  }

  //Vertical movement
  if (abs(photoBottomValue - photoTopValue) <= error) {
    Serial.print("Vert light difference within limit: " + String(error) + ". Do nothing. ");
    doVerticalMovement = false;
  } else {
    if(photoBottomValue > photoTopValue) {
    //left is brighter than right, move left, move to the light
      servoVerticalAngle -= resolution;
      Serial.print("Servo vert angle: " + String(servoVerticalAngle) + ", turn up. ");
    } else if (photoBottomValue < photoTopValue) {
      servoVerticalAngle += resolution;
      Serial.print("Servo vert angle: " + String(servoVerticalAngle) + ", turn down. ");
    }

    //Write to vertical servo
    if (servoVerticalAngle <= 0) {
      servoVerticalAngle = 0;
      doVerticalMovement = false;
      Serial.print("servo vertical angle = 0, do nothing. ");
    } else if (servoVerticalAngle >= 140) { //limit the rotation angle of servo for top to max 135 degree
      servoVerticalAngle = 140;
      doVerticalMovement = false;
      Serial.print("servo vertical angle = 140, do nothing. ");
    } else {
      doVerticalMovement = true;
      servoVertical.write(servoVerticalAngle);
      delay(m_speed);
    }
  }

  //If any change proposal has been done for both horizontal and vertical servo, set the turn delay to init value
  Serial.print("Hor move: " + String(doHorizontalMovement) + ", Vert move: " + String(doVerticalMovement) + ". ");
  if (doHorizontalMovement==1 || doVerticalMovement==1) {
    doSleep = false;
  } else {
    doSleep = true;
  }
}

void controlActuators() {

}

void controlLcd() {
  if (!digitalRead(interruptButtonPin)) {
    if (activateLcd == 0) {
      activateLcd = true;
      Serial.println("Set Activate LCD");
    } else {
      activateLcd = false;
      Serial.println("Set Deactivate LCD");
    }
  } else {
    Serial.println("test");
  }
}

void showLcdValues() {
  if (activateLcd==1) {
    lcd.backlight();

    char str1[5];
    char str2[3];
    char str3[3];
    char str4[3];
    char str5[3];

    dtostrf(lightSensorValue, -5, 0, str1); //Format the light value data as a string, left-aligned
    
    dtostrf(photoBottomValue, -3, 0, str2);
    dtostrf(photoTopValue, -3, 0, str3);
    dtostrf(photoLeftValue, -3, 0, str4);
    dtostrf(photoRightValue, -3, 0, str5);
    //LCD1602 display
    //display the value of the light intensity
    lcd.setCursor(0, 0);
    lcd.print("Light:");
    lcd.setCursor(6, 0);
    lcd.print(str1);
    lcd.setCursor(11, 0);
    lcd.print("lux");

    lcd.setCursor(0, 1);
    lcd.print("L:");
    lcd.setCursor(2, 1);
    lcd.print(str4);
    lcd.setCursor(6, 1);
    lcd.print("R:");
    lcd.setCursor(8, 1);
    lcd.print(str5);

  } else {
    lcd.noBacklight();
  }
}

void handleSleep() {
  if (doSleep==1) {
    Serial.print("Sleep " + String(sleepTime));
    LowPower.powerDown(sleepTime, ADC_OFF, BOD_OFF);
  } else {
    Serial.print("delay " + String(turnDelay));
    delay(turnDelay);
  }
}
