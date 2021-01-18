#include <Arduino.h>
#include <EEPROM.h>

#include <SPI.h>
#include <SD.h>     //sd card library
#include "RTClib.h" // Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <LiquidCrystal_I2C.h>
#include <Wire.h> // include the  I2C library code and Wire.h:

/*----------------------------------------------------------------------
------------------------------------------------------------------------
-------------------           Variables           ----------------------
------------------------------------------------------------------------
----------------------------------------------------------------------*/

/* ---- Library Objects -------*/
RTC_DS3231 rtc;                     //initialize rtc object
LiquidCrystal_I2C lcd(0x27, 16, 2); //initialize LCD object
File myFile;                        //initialize SD object
DateTime now;                       //initialize DateTime object

/* ----Laser/Button------*/
const int BUTTON_PIN = 8;   //using button instead of laser interrupt for debugging. (button is connected to 5v, when pressed pin will read high)
bool buttonState = LOW;     //track current state of button for debouncing and event triggering purposes
int buttonDebounceMS = 50;  //Minimum time for button state to remain the same before registering as change
bool lastButtonState = LOW; //temporary holder for debouncing trigger events

const int pinLaser = 4;     // output signal pin of laser module/laser pointer
const int pinReceiver = 3;  // input signal pin of receiver/detector (the used module does only return a digital state)
bool laserState = HIGH;     //Current state of laser, high means uninterrupted
bool lastLaserState = HIGH; //temp holder for "debouncing laser"
int laserDebounceMS = 200;  //Minimum time for laser to be interrupted for it to count.

/*----- EEPROM -------*/
const int COUNT_ADDR1 = 0; //use constants to define two memory addresses of data within the EEPROM to store values greater than 255
const int COUNT_ADDR2 = 1;
int eepromValue = 0;          //this will store the current number in EEPROM on bootup.
int valuesBetweenWrites = 10; //number of counts before writing to EEPROM

/* -----Counting Triggers ------*/
int totalCount = 0; //how many times the counter has been triggered
int writeCount = 0; //how many times since last EEPROM write
unsigned long lastDebounceTimeBtn = 0;
unsigned long lastDebounceTimeLsr = 0;
bool countIncremented = false; //flag to know if the current trigger was counted already (solves the "horse" problem)

/* -----Function Prototypes -----*/
//just declaring them up here so the compiler is aware of them, detail below main code
void watchForTrigger(); //this will be running constantly in loop to watch for laser/button press and set the flags if that happens
void handleTriggers();  //this will be running constantly in loop to handle when trigger events occur
void writeToEEPROM();   //write current total count to eeprom address
void saveToSD();        //write data to SD card
void updateLCD();       //update LCD with current count and time
void printToSerial();   //print data to serial port for debugging

void setup()
{
  Serial.begin(9600); // initialize serial communication at 9600 bits per second:
  Serial.print("Reading From EEPROM...");
  byte hiByte = EEPROM.read(COUNT_ADDR1);  //call from EEPROM the first half of our two byte data
  byte lowByte = EEPROM.read(COUNT_ADDR2); //call from EEPROM the second half of our two byte data
  eepromValue = word(hiByte, lowByte);     //join together the two 8-bit bytes for one 16-bit int using the word() function
  Serial.print("EEPROM Previously Stored value was: ");
  Serial.print(eepromValue);

  totalCount = eepromValue; //set count to value stored in eeprom

  Serial.print("Initializing SD card...");
  if (!SD.begin(10))
  {
    Serial.println("initialization failed!");
    while (1)
      ;
  }

  pinMode(BUTTON_PIN, INPUT);   //initialize button pin to input mode
  pinMode(pinLaser, OUTPUT);    // set the laser pin to output mode
  pinMode(pinReceiver, INPUT);  // set the laser pin to output mode
  digitalWrite(pinLaser, HIGH); //turn on the laser on boot
}

void loop()
{
  if (true) //laser on if before 7pm
  {
    watchForTrigger(); // Wait for trigger event, on event, set flag
    handleTriggers();  // If a flag is set, do something
    // updateLCD();       // enable this to update the lcd everytime the program loops, will result in flickering most likely
  }
  else
  {
    //eventual shutdown based on time of day
  }
}

/* ------ Trigger Monitor ---------*/
//this function watches button and laser for a change in state that lasts longer than "buttonDebounceMS" or "laserDebounceMS"
//this allows you to set a minimum time in change in state for it to register. I.e. if a bird fly's by and its only a 100ms event, we don't want to count it.
//if the amount of time for the change in state is greater than the set thresholds, we change the buttonState or laserState flags, to trigger the handler function
//Summary: this function is responsible for watching everything, always, and setting the states, the next function handleTriggers does stuff based on those states
void watchForTrigger()
{
  bool buttonRead = digitalRead(BUTTON_PIN); //Store Reading at beginning of function firing
  bool laserRead = digitalRead(pinReceiver); //Store Reading at beginning of function firing

  if (buttonRead != lastButtonState) //check if the state is the same or different than it was before, if different, reset lastDebounceTime to start a comparative timer
  {
    lastDebounceTimeBtn = millis();
  }

  if (laserRead != lastLaserState) //check if the state is the same or different than it was before, if different, reset lastDebounceTime to start a comparative timer
  {
    lastDebounceTimeLsr = millis();
  }

  if ((millis() - lastDebounceTimeBtn) > buttonDebounceMS) //if the time between the last "change of state" and now is greater than the defined debouce, continue with setting button state
  {
    if (buttonRead != buttonState)
    {                           //only change button state if it is different than it was
      buttonState = buttonRead; //sets state high or low, depending on status when read
    }
  }

  if ((millis() - lastDebounceTimeLsr) > laserDebounceMS) //if the time between the last "change of state" and now is greater than the defined debouce, continue with setting laser state
  {
    if (buttonRead != buttonState)
    {                           //only change laser state if it is different than it was
      buttonState = buttonRead; //sets state high or low, depending on status when read
    }
  }

  lastButtonState = digitalRead(BUTTON_PIN); //create a temporary flag of the state of the button every time function executes
  lastLaserState = digitalRead(pinReceiver); //create a temporary flag of the state of the laser every time function executes
}

/*---------- Trigger Handlers ----------------*/
//when the watchForTrigger function sees a state change that meets the minimum threshold it will set a flag,
//this function watches for those flags, and executes functions accordingly
//if the button goes high, or the laser goes low, and the count has NOT been incremented (or recorded however you wanna look at it), we need to increment count,
//however if the button goes high, or laser low, and the count HAS already been incremented, we need to ignore it,
//we also increment writeCount to track how many times since we saved to EEPROM in case of power failure
//after we increment these values we set a flag, countIncremented, so that until the trigger goes low, and then high again, we don't count again, because it is the same event.
//if we see that the countIncremented is true, but the trigger is low, we clear that flag, so when the trigger comes high again, we count it
void handleTriggers()
{
  if ((buttonState == HIGH | laserState == LOW) && !countIncremented) //when the button goes high, or laser low, and the count has NOT been incremented
  {
    totalCount++;                          //increment total count
    writeCount++;                          //increment number of counts since last EEPROM write
    countIncremented = true;               //set a flag that this count has been handled
    updateLCD();                           //each time the count changes we update the LCD
    printToSerial();                       //print to serial when count is triggered
    if (writeCount >= valuesBetweenWrites) //if the number of counts since last EEPROM write is greater than or equal to the "valuesBetweenWrites"
    {
      writeToEEPROM(); //write the values to memory and clear writeCounter
    }
  }

  if ((buttonState == LOW | laserState == HIGH) && countIncremented) //wait for button to go low, or laser high, before clearing countIncremented flag, so we don't count the same thing twice
  {
    countIncremented = false; //clear flag
  }
}

/* ---- Action Functions -----*/
//these functions just break up the code into smaller more readable segments

//this handles writing the 16-bit value to the 2 8-bit memory registers in EEPROM
void writeToEEPROM()
{
  byte hi = highByte(totalCount); //split the current totalCount into two 8-bit bytes
  byte low = lowByte(totalCount);
  EEPROM.write(COUNT_ADDR1, hi); //write the two separate bytes into EEPROM at defined addresses
  EEPROM.write(COUNT_ADDR2, low);
  writeCount = 0; //reset number of counts since last save to EEPROM
}

//this records the count event time etc to the SD card
void saveToSD()
{
  myFile = SD.open("count.txt", FILE_WRITE);
  now = rtc.now();
  float cels = rtc.getTemperature();
  float far = cels * 1.8 + 32;
  if (myFile)
  {
    myFile.print(now.month(), DEC);
    myFile.print(',');
    myFile.print(now.day(), DEC);
    myFile.print(',');
    myFile.print(now.year(), DEC);
    myFile.print(",");
    myFile.print(now.hour(), DEC);
    myFile.print(':');
    myFile.print(now.minute(), DEC);
    myFile.print(':');
    myFile.print(now.second(), DEC);
    myFile.print(",");
    myFile.print(far); //loacal var might need to just get temp and convert in sheet
    myFile.print(",");
    myFile.print(totalCount);
    myFile.println(",");
    myFile.close();
  }
  else
  {
    // if the file didn't open, print an error:
    Serial.println("error opening count.txt");
  }
}

//this function updates the LCD with the current time and count
//if we put this function within the main loop, it will update the time everytime it loops
//conversly, if we place it inside the count event handler, it will show the time of the last count
void updateLCD()
{
  lcd.setCursor(0, 0);
  lcd.print("Count: ");
  lcd.print(totalCount);
  lcd.setCursor(0, 1);
  lcd.print(now.hour(), DEC);
  lcd.print(':');
  lcd.print(now.minute(), DEC);
  lcd.print(':');
  lcd.print(now.second(), DEC);
}

//this function prints everything thats going to the sd card, to the serial.
void printToSerial()
{
  Serial.print(now.month(), DEC);
  Serial.print(',');
  Serial.print(now.day(), DEC);
  Serial.print(',');
  Serial.print(now.year(), DEC);
  Serial.print(",");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.print(",");
  Serial.print(far);
  Serial.print(",");
  Serial.print(count);
  Serial.println(",");
}