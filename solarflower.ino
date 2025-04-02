//Drivers
//#include <Wire.h>
//#include <LiquidCrystal_I2C.h>  //Use panel
//LiquidCrystal_I2C lcd(0x27, 16, 2);   //Panel

//#include <BH1750.h>     //Light intensity meter
//#include <Wire.h>
//BH1750 lightMeter(0x23);

//#include <Servo.h>
//Servo servoHorizontal;  //define the name of the servo rotating right and left
//Servo servoVertical;    //efine the name of the servo rotating upwards and downwards

#include <LowPower.h>   //Use low power between the cycles to save power

//--- Constants for pins ---//
//const byte interruptButtonPin = 2;  //the pin of button;the corruption is disrupted
const byte servoHorizontalPin = 9;
const byte servoVerticalPin = 10;
const byte powerDeactivationPin = 4;
const byte servoPowerDeactivationPin = 5;
//const byte ledLight = 3;
//const byte buzzer = 6;
const int photoDown = A0;
const int photoLeft = A1;
const int photoUp = A2;
const int photoRight = A3;

const byte lightsensoraddress = 0x23;

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
const int minLightSensorValue = 0;  //Min light to actively search for light source
const int maxLightTurnDelay = 10000; //If a balance between the sensors could be found, sleep longer so save power
const int photoRightCalibration = -30;  //Balance sensors as they are not equally calibrated
const int minPhotoResistorSolarValue = 730; //The min average value of the photoresistors to be able to generate power with the PV

// Power
//const period_t sleepTime = SLEEP_8S;
const int initLoopDelay = 200;   //turn delay for the initialization
const uint16_t shortSleepTime = 10;
const uint16_t longSleepTime = 30;


//--- Global variables ---//
// Photo resistors
int photoDownValue = 0;
int photoLeftValue = 0;
int photoUpValue = 0;
int photoRightValue = 0;
byte error = initError;
unsigned int turnDelay = initLoopDelay;
//bool activateLcd = false;
bool errorState = false;
unsigned int lightSensorValue = 0;   //save the variable of light intensity 
unsigned int photoResistorAverageValue = 0;


// Servos
// Horizontal movement
bool doHorizontalMovement = true;
// Vertical movement
bool doVerticalMovement = true;
int servoHorizontalAngle = servoHorizontalInitAngle;
int servoVerticalAngle = servoVerticalInitAngle;

// Power
bool doSleep = false;
int sleepTime = shortSleepTime;
int longSleepCount = 1;


void setup() {
  //Init digital IO as low to save power
  /*for (int i=0; i<20; i++) {
    //Set all to output except the analog photoread points
    if (i != powerDeactivationPin && i != servoHorizontalPin && i != servoVerticalPin) {
      pinMode(i, OUTPUT);
    }
  }*/

  Serial.begin(9600); //define the serial baud rate
  Serial.println("Start system");

  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  //Wire.begin();
  //lightMeter.begin();
  //lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23);
  //lightMeter.configure(BH1750::CONTINUOUS_LOW_RES_MODE);
  
  //pinMode(interruptButtonPin, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(interruptButtonPin), controlLcd, FALLING); //xternal interrupt touch type is falling edge; adjust_resolution is interrupt service function ISR

  //pinMode(ledLight, OUTPUT);
  //pinMode(buzzer, OUTPUT);
  pinMode(powerDeactivationPin, OUTPUT);  //Power deactivation pin photoresistors
  pinMode(servoPowerDeactivationPin, OUTPUT); //Servo power deactivation pin

  //Set to init position
  //servopulse(servoHorizontalPin, servoHorizontalInitAngle, 0, 270);
  //delay(100);
  //servoVertical.write(servoVerticalAngle); //return to initial angle
  //servopulse(servoVerticalPin, servoVerticalInitAngle, 0, 90);
  //servoHorizontal.write(servoHorizontalInitAngle); //return to initial angle
  //delay(100);
  //servoVertical.write(servoVerticalInitAngle); //return to initial angle
  //delay(100);
  //pinMode(servoHorizontalPin, OUTPUT);//set the pin of the servo
  //pinMode(servoVerticalPin, OUTPUT);//set the pin of the servo
  //servoHorizontal.attach(servoHorizontalPin);  //link the servo to digital port 9
  //servoVertical.attach(servoVerticalPin);  //link the servo to digital port 10

  //Photo sensors
  pinMode(photoDown, INPUT); //set the mode of pin for phot resistors
  pinMode(photoLeft, INPUT);
  pinMode(photoUp, INPUT);
  pinMode(photoRight, INPUT);

  //LCD init
  //lcd.init();          // initialize the LCD

  //Read sensors init
  readPhotoSensors();
  //readLightIntensity();
  // Set actuators to init value
  controlActuators();

  //**Long sleep
  //**Setup the watchdog timer begin
  //WDTCSR = (24);  //change enable and WDE - also resets
  //WDTCSR = (33);  //prescalers only - get rid of the WDE and WDCE bit
  //WDTCSR |= (1 << 6); //enable interrupt mode

  //enable sleep
  //SMCR |= (1 << 2); //Power down mode
  //SMCR |= 1;  //Enable sleep
  //** Setup the watchdog timer end
  digitalWrite(powerDeactivationPin, HIGH); //Write high to activate devices
  digitalWrite(servoPowerDeactivationPin, HIGH); //Write high to activate devices
  delay(1000);
}

void loop() {
  //Read photo sensors
  readPhotoSensors();
  //Read light intensity of PV
  //readLightIntensity();
  checkErrorState();
  //Reason on how to proceed
  if (activateMovement==true && errorState==0) {
    reasonAboutNextSteps();
    //Servo action
    controlActuators();
  }
  //Show LCD values
  //showLcdValues();
  //Delay for activity
  handleSleep();

  Serial.println();
  delay(100);
}

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
  photoDownValue = analogRead(photoDown);
  photoLeftValue = analogRead(photoLeft);
  photoUpValue = analogRead(photoUp);
  photoRightValue = analogRead(photoRight) + photoRightCalibration;

  photoResistorAverageValue = (int)(photoDownValue + photoLeftValue + photoUpValue + photoRightValue)/4;

  Serial.print("PUp: " + String(photoUpValue) + ", PDown: " + String(photoDownValue) + "| PLeft: " + 
    String(photoLeftValue) + ", PRight: " + String(photoRightValue) + " (" + String(-photoRightCalibration) + ")| Average: " + String(photoResistorAverageValue) + "|");
}

/**
* Read light sensor value
*/
/*void readLightIntensity() {
  lightSensorValue = lightMeter.readLightLevel();
  Serial.print("Light: " + String(lightSensorValue) + " lux. ");
}*/

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
    error = lowLightError;
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
  //Serial.print("Hor move: " + String(doHorizontalMovement) + ", Vert move: " + String(doVerticalMovement) + ". ");
  if (doHorizontalMovement==1 || doVerticalMovement==1) {
    doSleep = false;
  } else {
    doSleep = true;
  }
}

//Attach controllers and set the control positons
void controlActuators() {
  if (doHorizontalMovement==1) {
    setServoHorizontalAngle(servoHorizontalAngle);
  } 
  
  if (doVerticalMovement==1) {
    setServoVerticalAngle(servoVerticalAngle);
  }  
}

//React on button. Put no logic here as it may freeze the system
/*void controlLcd() {
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
}*/

/*void showLcdValues() {
  if (activateLcd==1) {
    lcd.on();
    lcd.backlight();

    char str1[5];
    char str2[4];
    char str3[4];
    char str4[4];
    char str5[4];

    dtostrf(lightSensorValue, -5, 0, str1); //Format the light value data as a string, left-aligned
    
    dtostrf(photoBottomValue, -4, 0, str2);
    dtostrf(photoTopValue, -4, 0, str3);
    dtostrf(photoLeftValue, -4, 0, str4);
    dtostrf(photoRightValue, -4, 0, str5);
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
    lcd.setCursor(8, 1);
    lcd.print("R:");
    lcd.setCursor(10, 1);
    lcd.print(str5);

  } else {
    lcd.noBacklight();
    lcd.off();
  }
}*/

void handleSleep() {
  if (doSleep==1) {
    Serial.print("Sleep " + String(sleepTime));
    digitalWrite(powerDeactivationPin, LOW);  //Write to deactivate all devices set by the transistor

    //Detach servo
    //servoHorizontal.detach();
    //servoVertical.detach();

    longSleepCount = longSleepCount + 1;  //Increment the long sleep count
    delay(500); //Else TX led is on
    longSleep(sleepTime);
    digitalWrite(powerDeactivationPin, HIGH); //Write high to activate devices
  } else {
    //Enable ADC again 
    //ADCSRA |= (1 << 7);

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
  /*for (int i=0;i<1;i++) {
    //Disable ADC with ~
    ADCSRA &= ~(1 << 7);
    // BOD disable
    MCUCR |= (3 << 5);
    MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6);  //then set the BODS bit and clear BODSE bit at the same time
    __asm__ __volatile__("sleep");
  }*/
      
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

//Call watchdog interrupt
//ISR(WDT_vect) {

//} // watchdog interrupt
