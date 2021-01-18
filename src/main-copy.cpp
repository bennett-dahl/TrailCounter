#include <Arduino.h>

#include <SPI.h>
#include <SD.h> //sd card library
// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include "RTClib.h"
//set variable for rtc
//need to review this to find out about I2c in parallel and run off a4 and a5 for nano
RTC_DS3231 rtc;
// include the  I2C library code and Wire.h:
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
File myFile;

DateTime now;
const int pinLaser = 4;    // output signal pin of laser module/laser pointer
const int pinReceiver = 3; // input signal pin of receiver/detector (the used module does only return a digital state)
int count = 0;             //how many times the counter has been triggered
// debounce variableint buttonState = LOW;
//debounce var int lastButtonState = LOW;
//debounce var unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
// debounce variable unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
void saveCount()
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
    myFile.print(count);
    myFile.println(",");
    myFile.close();
  }
  else
  {
    // if the file didn't open, print an error:
    Serial.println("error opening count.txt");
  }
}

void setup()
{
  pinMode(pinLaser, OUTPUT);   // set the laser pin to output mode
  pinMode(pinReceiver, INPUT); // set the laser pin to output mode
  Serial.begin(9600);          // initialize serial communication at 9600 bits per second:
  Serial.print("Initializing SD card...");
  if (!SD.begin(10))
  {
    Serial.println("initialization failed!");
    while (1)
      ;
  }
  lcd.init(); // initialize the LCD
  //turn on backlight
  lcd.backlight();
  //lcd.noBacklight();
  // make the pushbutton's pin an input:
  //pinMode(pushButton, INPUT);
  // Print a message to the LCD.
  lcd.print("Count: ");
}

void loop()
{
  int laser = digitalRead(pinReceiver); // receiver/detector send either LOW or HIGH
  float cels = rtc.getTemperature();
  float far = cels * 1.8 + 32;
  DateTime now = rtc.now();
  int h = now.hour();

  if (count < 10)
  { //laser on if before 7pm
    digitalWrite(pinLaser, HIGH);

    if (laser == LOW)
    {

      //increase count
      count++;
      lcd.setCursor(0, 0);
      lcd.print("Count: ");
      lcd.print(count);
      lcd.setCursor(0, 1);
      lcd.print(now.hour(), DEC);
      lcd.print(':');
      lcd.print(now.minute(), DEC);
      lcd.print(':');
      lcd.print(now.second(), DEC);

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
      saveCount();
      delay(2000);
    }
  }
  else
  {
    digitalWrite(pinLaser, LOW); //turn off laser at night
    Serial.println("Reached Max Count");
    delay(60000);
  }
}
