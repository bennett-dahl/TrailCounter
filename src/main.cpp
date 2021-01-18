#include <Arduino.h>
#include <EEPROM.h>

/* ----Laser/Button------*/
const int BUTTON_PIN = 8;   //using button instead of laser interrupt
bool buttonState = LOW;     //track current state of button/laser for debouncing and event triggering purposes
int buttonDebounceMS = 50;  //number of ms to wait for a "high read" before counting as a legitimate button press.
bool lastButtonState = LOW; //temporary holder for debouncing trigger events

/*----- EEPROM -------*/
const int COUNT_ADDR1 = 0; //use constants to define two memory addresses of data within the EEPROM to store values greater than 255
const int COUNT_ADDR2 = 1;

int eepromValue = 0;          //this will store the current number in EEPROM on bootup, as well as the data to be written to EEPROM just before a write.
int valuesBetweenWrites = 10; //number of counts before writing to EEPROM

/* -----Counting Triggers ------*/
int totalCount = 0; //how many times the counter has been triggered
int writeCount = 0; //how many times since last EEPROM write
unsigned long lastDebounceTime = 0;
bool countIncremented = false; //flag to know if the current trigger was counted already (solves the "horse" problem)

/* ---Function Prototypes -----*/
//just declaring them up here so the compiler is aware of them, detail below main code
void watchForTrigger(); //this will be running constantly in loop to watch for laser/button press and set the flags if that happens
void handleTriggers();  //this will be running constantly in loop to handle when trigger events occur
void writeToEEPROM();   //write current total count to eeprom address

void setup()
{
  Serial.begin(9600); // initialize serial communication at 9600 bits per second:
  Serial.print("Reading From EEPROM...");
  byte hiByte = EEPROM.read(COUNT_ADDR1);  //call from EEPROM the first half of our two byte data
  byte lowByte = EEPROM.read(COUNT_ADDR2); //call from EEPROM the second half of our two byte data
  eepromValue = word(hiByte, lowByte);     //join together the two 8-bit bytes for one 16-bit int using the word() function
  Serial.print("EEPROM Previously Stored value was: ");
  Serial.print(eepromValue);

  pinMode(BUTTON_PIN, LOW); //initialize button pin to low
}

void loop()
{
  watchForTrigger(); // Wait for trigger event
  handleTriggers();  // If a flag is set, do something

  if (1 == 1)
  { //laser on if before 7pm
    if (buttonState == LOW)
    {

      //increase count
      count++;
      delay(2000);
    }
  }
  else
  {
    //eventual shutdown based on time of day
  }
}

void watchForTrigger() //this function watches pin 8 for a change in state that lasts longer than "buttonDebounceMS"
{
  lastButtonState = digitalRead(BUTTON_PIN); //create a temporary flag indicating the trigger was activated

  if (digitalRead(BUTTON_PIN) != lastButtonState)
  { //watch for button/laser hasn't changed states too quickly
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > buttonDebounceMS)
  {                                        //if the time between the last "change of state" and now is greater than the defined debouce, continue with setting button state
    buttonState = digitalRead(BUTTON_PIN); //sets state high or low, depending on current actual status
  }
}

void handleTriggers()
{
  if (buttonState == HIGH && !countIncremented)
  {                          //when the button goes high, and the count has NOT been incremented
    totalCount++;            //increment total count
    writeCount++;            //increment number of counts since last EEPROM write
    countIncremented = true; //set a flag that this count has been handled
    if (writeCount >= valuesBetweenWrites)
    {                  //if the number of counts since last EEPROM write is greater than or equal to the "valuesBetweenWrites"
      writeToEEPROM(); //write the values to memory and clear writeCounter
    }
  }

  if (buttonState == LOW && countIncremented)
  {                           //wait for button to go low again before clearing countIncremented flag, so we don't count the same thing twice
    countIncremented = false; //clear flag
  }
}

void writeToEEPROM()
{
  byte hi = highByte(totalCount); //split the current totalCount into two 8-bit bytes
  byte low = lowByte(totalCount);
  EEPROM.write(COUNT_ADDR1, hi); //write the two separate bytes into EEPROM at defined addresses
  EEPROM.write(COUNT_ADDR2, low);
  writeCount = 0; //reset number of counts since last save to EEPROM
}

bool debounce(int16_t pin)
{
  unsigned long downTime = millis();
  if (digitalRead(pin))
  {
  }
}