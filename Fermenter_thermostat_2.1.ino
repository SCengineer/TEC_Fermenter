/* SD card and logging need to be checked
 * adding current sensor at pin 30.. need to test
 * Delay relays to give the pump and fan more time then the TEC..  need to test
 * redo 4x20 display output?
 */

#include <OneWire.h> //temp sensors
#include <DallasTemperature.h> //temp sensors
#include <LiquidCrystal.h>
#include <SPI.h> //SD Card
#include <SD.h> //SD Card
#include <Wire.h> //RTC 
#include "RTClib.h" //RTC

LiquidCrystal lcd(15, 14, 5, 4, 3, 2); // initialize the library with the numbers of the interface pins.  

RTC_PCF8523 rtc; //RTC chip 

#define ONE_WIRE_BUS 8
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature. 
DeviceAddress fermentThermometer = { 0x28, 0xF8, 0x9A, 0x26, 0x00, 0x00, 0x80, 0x32, };
DeviceAddress waterThermometer = { 0x28, 0x75, 0x16, 0x27, 0x00, 0x00, 0x80, 0xF7 };
DeviceAddress airThermometer = { 0x28, 0xA6, 0xBB, 0x26, 0x00, 0x00, 0x80, 0x3A };
//DeviceAddress airThermometer = { 0x28, 0x59, 0xBE, 0xDF, 0x02, 0x00, 0x00, 0x9F };

//Set up variables using the SD utility library functions:
//Note the SPI uses 50, 51, 52 on Mega and 11, 12, and 13 on other boards 
Sd2Card card;
SdVolume volume;
SdFile root;
const int chipSelect = 10;

//const int potPin = A1;
const int swPin1 = 36;
const int swPin2 = 38; //only need one pin of of the switch
const int tecPin = 24; //drives the DC power supply to the TEC. 
const int heatPin = 26; //determined heat or chill.  LOW is heat.
const int dcPin = 22; // drives 
const int buttonUp = 7;
const int buttonDown = 6;

const int currentPin = A0;
int mVperA =100;  //for -20 to 20A ACS712 chip
int ACSoffset = 2500;
int currentRaw = 0;
float currentDC = 0;
float currentA = 0;

int menuState = 0;
int tempState = HIGH;
int swStatus1 = HIGH;
int swStatus2 = HIGH;
int dcState = HIGH;  //controls 12vdc fans and pumps
int tecState = HIGH; //24vdc for TECs
int heatDirection = HIGH; //feed - or + to TEC for heat or cool.  Chill is
float tempSetLow = 50.0;
float tempSetHigh = 75.0;
float hyst = 0.3;
float tempFerment;
float tempWater;
float tempAir;

unsigned long previousMillis = 0; //will store last time sensors were updated
const unsigned long sensorInterval = 3000; //time between updating the sensors
unsigned long previousLog = 0; //stores last time the log was updated
const unsigned long logInterval = 60000; //time between logs

String year, month, day, hour, minute, second, time, date;

void setup() {
  digitalWrite(tecPin, HIGH);
  digitalWrite(heatPin, HIGH);
  digitalWrite(dcPin, HIGH);
  
  pinMode(dcPin, OUTPUT);
  pinMode(tecPin, OUTPUT);
  pinMode(heatPin, OUTPUT);
  digitalWrite(tecPin, HIGH);
  digitalWrite(heatPin, HIGH);
  digitalWrite(dcPin, HIGH);
  
  pinMode(buttonUp, INPUT);
  pinMode(buttonDown, INPUT);
  pinMode(swPin1, INPUT);
  pinMode(swPin2, INPUT);
  //pinMode(potPin, INPUT);
  pinMode(currentPin, INPUT);


 
  Serial.begin(57600);
  lcd.begin(20,4); // set up the LCD's number of columns and rows:
  lcd.print("Fermenter V2.0.1"); // Print a message to the LCD.
  delay(2000);
  
  while (!Serial) {
  delay(1);  // for Leonardo/Micro/Zero
  }
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (! rtc.initialized()) {
    Serial.println("RTC is NOT running!");
  }
  else {
    Serial.println("RTC good to go");
  }
    //if (rtc.lostPower()) {
    //Serial.println("RTC lost power, lets set the time!");
    //following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  DateTime now = rtc.now();  
  year = String(now.year(), DEC);
  month = String(now.month(), DEC);
  day = String(now.day(), DEC);
  hour = String(now.hour(), DEC);
  minute = String(now.minute(), DEC);
  second = String(now.second(), DEC);
  date = year + "/" + month + "/" + day;
  time = hour + ":" + minute + ":" + second; 

  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    return;
  }  
  else {
    Serial.println("card initialized.");
    Serial.print(date); 
    Serial.print(",");
    Serial.println(time);

    File testLog = SD.open("ferment.log", FILE_WRITE);
    testLog.println(date);   
    testLog.print(",");
    testLog.println(time);
    testLog.println("date, time, temp set low, temp set high, hyst, fermentT, waterT, airT, state");
    testLog.close();
    return;    
  }
  
  // set the resolution to 10 bit (good enough?)
  sensors.setResolution(fermentThermometer, 10);
  sensors.setResolution(waterThermometer, 10);
  sensors.setResolution(airThermometer, 10);
  sensors.begin();
}

//function for calling the temperature
void printTemperature(DeviceAddress deviceAddress){
  float tempF = sensors.getTempF(deviceAddress);
  if (tempF == -168.0) {
    lcd.print("Err");
  } else {  
    lcd.print(tempF, 1); // "F: xx.x"  could be 6-8 digits.
    lcd.print("F");  
  }
}

//call for the temp every 5 seconds and update the LCD.  Persistant 
void readTemp(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= sensorInterval) {
    previousMillis = currentMillis;
      
    lcd.setCursor(0, 1);
    lcd.print("Ferme: ");
    printTemperature(fermentThermometer);
    tempFerment = sensors.getTempF(fermentThermometer);
      //float potValue = analogRead(potPin);  //for use as a ferment temp simulator
      //tempFerment = potValue/6;
    
    lcd.setCursor(0, 2);
    lcd.print("Water: ");
    printTemperature(waterThermometer);
    tempWater = sensors.getTempF(waterThermometer);

    lcd.setCursor(0, 3);
    lcd.print("Ambie: ");
    printTemperature(airThermometer);
    tempAir = sensors.getTempF(airThermometer);
    
    sensors.requestTemperatures(); 
  }
}

void controlTemp(){
  //decide between chill, heat, or off [1, 2, 0] 
  //only do this every couple of seconds?
  //

  int lastTempState = tempState;  //need to determine if the temp state is changing.  
  
  if (tempFerment <= (tempSetLow - hyst)){
    tempState = 2; //heat
  }
  if (tempFerment >= (tempSetHigh + hyst)){
    tempState = 1; //chill
  }
  if ((tempFerment > (tempSetLow + hyst)) && (tempFerment < (tempSetHigh - hyst))){
    tempState = 0; //off
  } 
  if (tempState == 2 && tempFerment <= (tempSetLow + hyst) && tempFerment >= (tempSetLow - hyst)){
    tempState = 2; //heat    
  } 
  if (tempState == 0 && tempFerment <= (tempSetLow + hyst) && tempFerment >= (tempSetLow - hyst)){
    tempState = 0; //off
  }  
  if (tempState == 1 && tempFerment >= (tempSetHigh - hyst) && tempFerment <= (tempSetHigh + hyst)){
    tempState = 1; //chill    
  } 
  if (tempState == 0 && tempFerment >= (tempSetHigh - hyst) && tempFerment <= (tempSetHigh + hyst)){
    tempState = 0; //off    
  } 

  if (lastTempState != tempState) {
    if (tempState == 0){ //off
      dcState = 1;
      tecState = 1;
      heatDirection = 1;
    }
    if (tempState == 1){ //chiller
      dcState = 0;
      tecState = 0;
      heatDirection = 1;
    }
    if (tempState == 2){ //heater
      dcState = 0;
      tecState = 0;
      heatDirection = 0;
    }  

    digitalWrite(tecPin, HIGH); //turn off power to TEC.  Don't want to creat a short when switching 
    delay(200);
    digitalWrite(dcPin, dcState);
    digitalWrite(heatPin, heatDirection);
    delay(200);
    digitalWrite(tecPin, tecState);

    //Update the LCD
    if (tempState == 2){
      lcd.setCursor(15, 2);
      lcd.print("HEAT");
    }
    else if (tempState == 1){
      lcd.setCursor(15, 2);
      lcd.print("CHILL");
    }
    else if (tempState == 0){
      lcd.setCursor(15, 2);
      lcd.print("OFF ");
    }
  }
}

void logTemp(){
  unsigned long currentLog = millis();
    if (currentLog - previousLog >= logInterval) {
      previousLog = currentLog;
  
    DateTime now = rtc.now();  

    File fermentLog = SD.open("ferment.log", FILE_WRITE);
    if (fermentLog){
      fermentLog.print(now.year(), DEC);
      fermentLog.print('/');
      fermentLog.print(now.month(), DEC);
      fermentLog.print('/');
      fermentLog.print(now.day(), DEC);
      fermentLog.print(",");
      fermentLog.print(now.hour(), DEC);
      fermentLog.print(':');
      fermentLog.print(now.minute(), DEC);
      fermentLog.print(':');
      fermentLog.print(now.second(), DEC);
      fermentLog.print(","); 
      fermentLog.print(tempSetLow);
      fermentLog.print(",");
      fermentLog.print(tempSetHigh);
      fermentLog.print(",");
      fermentLog.print(hyst);
      fermentLog.print(",");
      fermentLog.print(tempFerment);
      fermentLog.print(",");
      fermentLog.print(tempWater);
      fermentLog.print(",");
      fermentLog.print(tempAir);
      fermentLog.print(",");
      fermentLog.print(tempState);
      fermentLog.print(",");
      fermentLog.print(currentDC);
      fermentLog.println();
      fermentLog.close();
    }
    else {
      Serial.println("No SD card");
    }
    Serial.print(now.year(), DEC);
    Serial.print('.');
    Serial.print(now.month(), DEC);
    Serial.print('.');
    Serial.print(now.day(), DEC);
    Serial.print(",");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.print(","); 
    Serial.print(tempSetLow);
    Serial.print(",");
    Serial.print(tempSetHigh);
    Serial.print(",");
    Serial.print(hyst);
    Serial.print(",");
    Serial.print(tempFerment);
    Serial.print(",");
    Serial.print(tempWater);
    Serial.print(",");
    Serial.print(tempAir);
    Serial.print(",");
    Serial.print(tempState);
    Serial.print(",");
    Serial.print(currentDC);
    Serial.println();
    //EEPROM.put (0,tempSet);
    //EEPROM.put (2, hyst); 
  }
}

void checkButtons(){
  swStatus1 = digitalRead(swPin1);
  swStatus2 = digitalRead(swPin2);

  //To prevent getting in a situation where the setpoints become illogical
  float maxLowTemp = tempSetHigh - hyst - 0.1; 
  float minHighTemp = tempSetLow + hyst + 0.1;
  
  int buttonUpState = digitalRead(buttonUp);  
  int buttonUpLast = 0; //previous state of the button
  if (buttonUpState != buttonUpLast  && swStatus1 == LOW) {
    tempSetLow = tempSetLow + .1;
    if (tempSetLow >= maxLowTemp) {
      tempSetLow = maxLowTemp;
    }
    delay(300);
  }
  if (buttonUpState != buttonUpLast  && swStatus1 == HIGH) {
    tempSetHigh = tempSetHigh + .1;
    delay(300);
  }
  buttonUpLast = buttonUpState;
  
  int buttonDownState = digitalRead(buttonDown);
  int buttonDownLast = 0; 
  if (buttonDownState != buttonDownLast && swStatus1 == LOW) {
    tempSetLow = tempSetLow - .1;
    delay(200);
  }
  if (buttonDownState != buttonDownLast && swStatus1 == HIGH) {
    tempSetHigh = tempSetHigh - .1;
    if (tempSetHigh <= minHighTemp) {
        tempSetHigh = minHighTemp;  
      }
    delay(200);
  }
  buttonDownLast = buttonDownState;  
}

void readCurrent() {
//read the current from the ACS712 breakout board.
//Either all DC voltage, or just the TEC voltage passes through
//2.5v is 0.  100mv/a sensitivity for the 20A version.    
//average anything?

  currentRaw = analogRead(currentPin);  //gives 0 - 1024? 
  currentDC = (currentRaw / 1024.0) * 5000; //now 0 - 5000mV
  currentA = ((currentDC - ACSoffset) / mVperA); //hopefully -20 to 20
}

void updateScreen(){
  //maintain tempSet on LCD
  lcd.setCursor(0, 0);
  lcd.print("L:");
  lcd.print(tempSetLow, 1);
  lcd.print(" H:");
  lcd.print(tempSetHigh, 1);
  lcd.print(" h:");
  lcd.print(hyst, 1);

  //show TEC current reading on LCD 
  lcd.setCursor(15, 3);
  lcd.print(currentA, 1);
}

void loop() {

  //only run these functions if not interacting with menu
  if (menuState == 0) {
    updateScreen(); //block
    readTemp(); //block
    controlTemp(); //block
    readCurrent();
  }

  //always run these functions
  checkButtons();
  logTemp();

}

