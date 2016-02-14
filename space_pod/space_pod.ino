#include <CircularLED.h>
#include <Grove_LED_Bar.h>
#include <ChainableLED.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <ArduinoJson.h>
#include <AnimatedCircularLED.h>

#define DEBUG

#define HULL_DAMAGE (0x1)
#define CABIN_PRESSURE (0x2)
#define OXYGEN_LOW (0x4)
#define FUEL_LOW (0x8)
#define ENGINE_MALFUNCTION (0x16)
#define FUSE (0x32)
#define COLLISION (0x64)
#define FUEL_LINE (0x128)

// Devices
#define NUM_STATUS_LEDS  2
ChainableLED status_leds(6, 7, NUM_STATUS_LEDS);
rgb_lcd lcd;
Grove_LED_Bar bar(9, 8, 0); // Clock pin, Data pin, Orientation
CircularLED circularLED1(8,7);
AnimatedCircularLED animatedCircularLED;

const byte buttonPin = 3;
const int enginePowerMeter = A0;

// CONSTANTS
const float FLASH_DURATION = 1000;

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

float m_enginePowerInput = 0.0f; // Efects fuel level, malfunction chance
float m_enginePower = 0.0f; // Efects fuel level, malfunction chance
byte m_numAstronauts = 0; // Effects O2 production

unsigned long m_flashWarnTime;
unsigned long m_lastWarnTime;
bool m_isFlashWarnOn;

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
byte m_currentLCDState = s_lcdState;

bool m_buttonOn = false;
bool m_warningOn = false;
byte m_warningFlags;
unsigned long m_lastSerialPrint;
String m_inputString = "";

void setup() {
  // Configure the serial communication line at 9600 baud (bits per second.)
  Serial.begin(9600);
  
  // Configure the angle sensor's pin for input.
  pinMode(enginePowerMeter, INPUT);
  pinMode(buttonPin, INPUT);
  
  bar.begin();

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  
  lcd.setRGB(0, 0, 255);

  toggleLCDState();
  SetStatusRGB(0,0,0);
}

void loop() {
  readInput();
  readSerial();
  
  displayWarning();
  displayStatus();

  updateServer();
}

void readSerial() {
  while (Serial.available() > 0) {
    int inChar = Serial.read();
    m_inputString += (char)inChar;
     
    if (inChar == '\0') {
      //Serial.println(m_inputString);
      if (m_inputString.startsWith("reset")) {
         toggleLCDState();
      } else {
        StaticJsonBuffer<200> jsonReadBuffer;
        JsonObject& jsonInput = jsonReadBuffer.parseObject(m_inputString);
        if (jsonInput.success()){
          /*
          const char* sensor    = root["sensor"];
          long        time      = root["time"];
          double      latitude  = root["data"][0];
          double      longitude = root["data"][1];
          */
          if (jsonInput.containsKey("ep"))
          {
            m_enginePower = jsonInput["ep"];
          }
          
          if (jsonInput.containsKey("wf"))
          {
            m_warningFlags = jsonInput["wf"];
          }

          if (jsonInput.containsKey("fl"))
          {
            m_fuel = jsonInput["fl"];
          }
          
          // jsonInput.printTo(Serial);
        }
      }
      m_inputString = "";
    }
  }
}

void readInput() {
  // Read engine power level
  int value = analogRead(enginePowerMeter);
  m_enginePowerInput = (float) value / 1023;
  if (m_enginePowerInput >= 0.995f) m_enginePowerInput = 1;
  
  handleButton();
}

void handleButton() {
  bool buttonDown = digitalRead(buttonPin);
  if (buttonDown && !m_buttonOn)
  {
    m_buttonOn = true;
    toggleLCDState();
  } else if (!buttonDown) {
    m_buttonOn = false;
  }
}

void displayStatus() {
  if (!m_warningOn) {
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
  
  bar.setLevel(int(10 * m_fuel));
  animatedCircularLED.setPercentage(m_enginePower);
}

void displayWarning() {
  // Check fuel, buzz if engine power up and fuel low
  if (m_warningFlags > 0) {
    if (!m_warningOn) {
        lcd.clear();
        lcd.setRGB(255, 0, 0);
        String stringOne = String("WARNING!");
        lcd.setCursor(0, 0);
        lcd.print(stringOne);
        lcd.setCursor(0, 1);
        // TODO: Figure out which warning string to display
        //lcd.print(stringTwo);
      }

    // TODO: Flash warning LED and buzzer
    unsigned long currentTime = millis();
    unsigned long delta = currentTime - m_lastWarnTime;
    m_flashWarnTime += delta;
    if (m_isFlashWarnOn && m_flashWarnTime < FLASH_DURATION) {
      // Process something while flash warn on
    } else if (!m_isFlashWarnOn && m_flashWarnTime < FLASH_DURATION){
      // Process something while flash warn off
    } else {
      m_isFlashWarnOn = !m_isFlashWarnOn;
      m_flashWarnTime = 0;
      if (m_isFlashWarnOn) {
        SetStatusRGB(255,0,0);
      } else {
        SetStatusRGB(0,0,0);
      }
    }
    m_lastWarnTime = currentTime;
    m_warningOn = true;
  } else if (m_warningOn) {
    m_warningOn = false;
    lcd.setRGB(0, 0, 255);
    SetStatusRGB(0,0,0);
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

void updateServer() {
  unsigned long currentTime = millis();
  if(currentTime > m_lastSerialPrint + 100)
  {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["epi"] = m_enginePowerInput;
    root.printTo(Serial);
    m_lastSerialPrint = millis();
  }
}

void SetStatusRGB(int r, int g, int b) {
  for(int i=0; i < NUM_STATUS_LEDS; i++) {
    status_leds.setColorRGB(i, r, g, b);
  }
}
