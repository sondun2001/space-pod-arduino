#include <CircularLED.h>
#include <Grove_LED_Bar.h>
#include <ChainableLED.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <ArduinoJson.h>

//#define DEBUG

StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();

// Devices
#define NUM_STATUS_LEDS  2
ChainableLED status_leds;
const int statusLEDPinClk = 6;
const int statusLEDPinDta = 7;
rgb_lcd lcd;
Grove_LED_Bar bar(9, 8, 0); // Clock pin, Data pin, Orientation
CircularLED circularLED1(8,7);
const int buttonPin = 3;
const int speakerPin = 4;
const int enginePowerMeter = A0;

// CONSTANTS
const int ALARM_TONE = 1915;
const float FLASH_DURATION = 1000;

// What are the chances of something malfunctioning
// Will be multiplied by engine power
const float CHANCE_MALFUNCTION = 0.1f;
const float CHANCE_REPAIR = 0.1f;

#ifdef DEBUG
const float FUEL_BURN_RATE = 0.001;
#else
const float FUEL_BURN_RATE = 0.0001;
#endif

// Resources
float m_auxPower = 1.0f;
float m_fuel = 1.0f;
float m_oxygen = 1.0f;

bool m_powerOn = false;
bool m_isFueling = false;
bool m_isRepairing = false;

float m_enginePower = 1.0f; // Efects fuel level, malfunction chance
int m_numAstronauts = 0; // Effects O2 production

unsigned int ENGINE_POWER_LED[24];
unsigned int m_currentEnginePowerLed;
unsigned long m_lastEnginePowerDisplayTime;

unsigned long m_lastBuzzerTime;
unsigned long m_flashWarnTime;
bool m_isFlashWarnOn;
bool m_isBuzzerOn;

enum LCDState {
  LCD_INIT = -1,
  LCD_FUEL,
  LCD_ENGINE,
  LCD_OXYGEN,
  LCD_AUX,
  LCD_HULL,
  LCD_NUM_STATES
};

LCDState s_lcdState = LCD_INIT;
int m_currentLCDState = s_lcdState;

bool m_buttonOn = false;
bool m_fuelWarningOn = false;

unsigned long m_lastSerialPrint;

void setup() {
  // Configure the serial communication line at 9600 baud (bits per second.)
  Serial.begin(57600);
  
  // Configure the angle sensor's pin for input.
  pinMode(enginePowerMeter, INPUT);
  pinMode(speakerPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  
  bar.begin();

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  
  lcd.setRGB(0, 0, 255);

  toggleLCDState();

  circularLED1.CircularLEDWrite(ENGINE_POWER_LED);
  status_leds.ChainableRGBLEDWrite(statusLEDPinClk, statusLEDPinDta, NUM_STATUS_LEDS, 0, 0, 0);
}

void loop() {
  readInput();
  
  // put your main code here, to run repeatedly:
  simulateAux();
  simulateFuel();
  simulateOxygen();

  displayEngineLed();
  displayWarning();
  displayStatus();
}

void readInput() {
  if (m_fuel > 0) {
    int value = analogRead(enginePowerMeter);
    m_enginePower = (float) value / 1023;
    if (m_enginePower >= 0.995f) m_enginePower = 1;
  } else {
    m_enginePower = 0;
  }

  handleButton();
}

void handleButton() {
  bool buttonDown = digitalRead(buttonPin);
  if (buttonDown && !m_buttonOn)
  {
    m_buttonOn = true;

    // If fuel ran out, have button refuel
    if (m_fuel <= 0) {
      m_fuel = 1;
    } else {
      toggleLCDState();
    }
  } else if (!buttonDown) {
    m_buttonOn = false;
  }
}

void simulateAux() {
  
}

void simulateFuel() {
  m_fuel = m_fuel - (m_enginePower * FUEL_BURN_RATE);
  bar.setLevel(int(10 * m_fuel));
}

void simulateOxygen() {
  
}

void displayStatus() {
  if (!m_fuelWarningOn) {
    String stringOne;
    String stringTwo;
    switch(s_lcdState) {
      case LCD_FUEL:
        stringOne = String("ENGINE: ");
        stringTwo = String("FUEL: ");
        stringOne += (int)(m_enginePower * 100);
        stringTwo += (int)(m_fuel * 100);
        stringOne += "% ";
        stringTwo += "% ";
        lcd.setCursor(0, 0);
        lcd.print(stringOne);
        lcd.setCursor(0, 1);
        lcd.print(stringTwo);
        break;
    }
  }
  
  unsigned long currentTime = millis();
  if(currentTime > m_lastSerialPrint + 1000)
  {
    root["ep"] = m_enginePower;
    root.printTo(Serial);
    
    #ifdef DEBUG
    Serial.println(m_enginePower);
    Serial.println(s_lcdState);
    #endif
    m_lastSerialPrint = millis();
  }
}

void displayWarning() {

  // Check fuel, buzz if engine power up and fuel low
  if (m_fuel < 0.1f) {
    if (!m_fuelWarningOn) {
      lcd.clear();
      lcd.setRGB(255, 0, 0);
      String stringOne = String("WARNING!");
      String stringTwo = String("FUEL LOW");
      lcd.setCursor(0, 0);
      lcd.print(stringOne);
      lcd.setCursor(0, 1);
      lcd.print(stringTwo);
    }

    // TODO: Flash warning LED and buzzer
    unsigned long currentTime = millis();
    unsigned long delta = currentTime - m_lastBuzzerTime;
    m_flashWarnTime += delta;
    if (m_isFlashWarnOn && m_flashWarnTime < FLASH_DURATION) {
      playBuzzer();
    } else if (!m_isFlashWarnOn && m_flashWarnTime < FLASH_DURATION){
      // Process Something while flash warn off
    } else {
      m_isFlashWarnOn = !m_isFlashWarnOn;
      m_flashWarnTime = 0;
      if (m_isFlashWarnOn) {
        status_leds.ChainableRGBLEDWrite(statusLEDPinClk, statusLEDPinDta, NUM_STATUS_LEDS, 255, 0, 0);
      } else {
        digitalWrite(speakerPin, LOW);
        m_isBuzzerOn = false;
        status_leds.ChainableRGBLEDWrite(statusLEDPinClk, statusLEDPinDta, NUM_STATUS_LEDS, 0, 0, 0);
      }
    }
    m_lastBuzzerTime = currentTime;
    m_fuelWarningOn = true;
  } else if (m_fuelWarningOn) {
    m_fuelWarningOn = false;
    digitalWrite(speakerPin, LOW);
    lcd.setRGB(0, 0, 255);
    status_leds.ChainableRGBLEDWrite(statusLEDPinClk, statusLEDPinDta, NUM_STATUS_LEDS, 0, 0, 0);
  }
}

void playBuzzer() {
  if (micros() % 2 == 0) {
      if (!m_isBuzzerOn) 
      {
        digitalWrite(speakerPin, HIGH);
        m_isBuzzerOn = true;
      } else if (m_isBuzzerOn) {
        digitalWrite(speakerPin, LOW);
        m_isBuzzerOn = false;
      }
    }
}

void displayEngineLed() {
  // Read engine status and display engine power led
  unsigned int enginePower = (int)(24 * m_enginePower);
  unsigned long currentTime = millis();
  if (m_currentEnginePowerLed != enginePower && currentTime > m_lastEnginePowerDisplayTime + 50) {
    if (enginePower > m_currentEnginePowerLed) {
      for (int i=0;i<24;i++)
      {
        unsigned int ledOnOff = (i < enginePower) ? 0xff : 0;
        if (ENGINE_POWER_LED[i] != ledOnOff) {
          ENGINE_POWER_LED[i] = ledOnOff;
          m_currentEnginePowerLed = i;
          break;
        }
      }
    } else {
      for (int i=23;i>=0;i--)
      {
        unsigned int ledOnOff = (i < enginePower) ? 0xff : 0;
        if (ENGINE_POWER_LED[i] != ledOnOff) {
          ENGINE_POWER_LED[i] = ledOnOff;
          m_currentEnginePowerLed = i;
          break;
        }
      }
    }
    
    circularLED1.CircularLEDWrite(ENGINE_POWER_LED);
    m_lastEnginePowerDisplayTime = currentTime;
  }
}

void playTone(int tone, int duration) {
    for (long i = 0; i < duration * 1000L; i += tone * 2) {
        digitalWrite(speakerPin, HIGH);
        delayMicroseconds(tone);
        digitalWrite(speakerPin, LOW);
        delayMicroseconds(tone);
    }
}

void playNote(char note, int duration) {
    char names[] = { 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'C' };
    int tones[] = { 1915, 1700, 1519, 1432, 1275, 1136, 1014, 956 };

    // play the tone corresponding to the note name
    for (int i = 0; i < 8; i++) {
        if (names[i] == note) {
            playTone(tones[i], duration);
        }
    }
}

void toggleLCDState () {
  m_currentLCDState++;
  if (m_currentLCDState >= LCD_NUM_STATES) m_currentLCDState = LCD_FUEL;
  s_lcdState = (LCDState)m_currentLCDState;
  lcd.clear();
  switch(s_lcdState) {
    case LCD_FUEL:
      //
      break;
   default:
      lcd.setCursor(0, 0);
      lcd.print("WELCOME ABOARD");
      break;
  }
}

