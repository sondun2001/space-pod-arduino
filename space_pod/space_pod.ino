#include <CircularLED.h>
#include <Grove_LED_Bar.h>
#include <ChainableLED.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include <ArduinoJson.h>
#include <AnimatedCircularLED.h>
#include <math.h>

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
Grove_LED_Bar fuel_bar(3, 2, 0); // Clock pin, Data pin, Orientation
Grove_LED_Bar aux_bar(4, 3, 0);
CircularLED circularLED(8, 7);
AnimatedCircularLED animatedCircularLED(&circularLED);
const byte PIR_MOTION_SENSOR = 5;
const byte BUTTON = 8;
const int ENGINE_POWER_INPUT = A0;

// CONSTANTS
const float FLASH_DURATION = 1000;

// Resources
float m_auxLevel = 1.0f;
float m_fuel = 1.0f;
unsigned short m_chargeRate = 100;
unsigned short m_drainRate = 100;
byte m_oxygen = 0;
byte m_cabinPressure = 0;
 
bool m_powerOn = true;
bool m_isFueling = false;
bool m_isRepairing = false;

float m_enginePowerInput = 0.0f; // Efects fuel level, malfunction chance
float m_enginePower = 0.0f; // Efects fuel level, malfunction chance

unsigned long m_flashWarnTime;
unsigned long m_lastWarnTime;
bool m_isFlashWarnOn;
byte m_warningField;
bool m_warningOn = false;

enum LCDState {
  LCD_INIT = -1,
  LCD_ENGINE_FUEL,
  LCD_CHARGE_AUX,
  LCD_PRESSURE_OXYGEN,
  LCD_NUM_STATES
};

LCDState s_lcdState = LCD_INIT;
byte m_currentLCDState = s_lcdState;

bool m_buttonOn = false;
unsigned long m_lastSerialPrint;
String m_inputString = "";

unsigned long m_lastPeopleActivity;

void setup() {
  // Configure the serial communication line at 9600 baud (bits per second.)
  Serial.begin(9600);
  
  // Configure the angle sensor's pin for input.
  pinMode(PIR_MOTION_SENSOR, INPUT);
  pinMode(ENGINE_POWER_INPUT, INPUT);
  pinMode(BUTTON, INPUT);
  
  fuel_bar.begin();
  aux_bar.begin();
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  animatedCircularLED.setAnimationStepTime(100);
  
  toggleLCDState();
  setDefaultState();
}

void setDefaultState() {
  lcd.setRGB(0, 0, 255);
  SetStatusRGB(0, 255, 0);
}

void loop() {
  isPeopleDetected();
  
  if (m_powerOn) {
    readInput();
    readSerial();
    
    displayWarning();
    displayStatus();
    
    updateServer();
  }
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
          if (jsonInput.containsKey("ep")) { m_enginePower = jsonInput["ep"]; }
          if (jsonInput.containsKey("wf")) { m_warningField = jsonInput["wf"]; }
          if (jsonInput.containsKey("fl")) { m_fuel = jsonInput["fl"]; }
          if (jsonInput.containsKey("al")) { m_auxLevel = jsonInput["al"]; }
          if (jsonInput.containsKey("cr")) { m_chargeRate = jsonInput["cr"]; }
          if (jsonInput.containsKey("dr")) { m_drainRate = jsonInput["dr"]; }
        }
      }
      m_inputString = "";
    }
  }
}

void readInput() {
  // Read engine power level
  int value = analogRead(ENGINE_POWER_INPUT);
  m_enginePowerInput = (float) value / 1023;
  if (m_enginePowerInput >= 0.995f) m_enginePowerInput = 1;

  // Read Button
  bool buttonDown = digitalRead(BUTTON);
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
      case LCD_ENGINE_FUEL:
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
      case LCD_CHARGE_AUX:
        stringOne = String("CHARGE: ");
        stringTwo = String("DRAIN: ");
        stringOne += m_chargeRate;
        stringTwo += m_drainRate;
        stringOne += "W  ";
        stringTwo += "W  ";
        lcd.setCursor(0, 0);
        lcd.print(stringOne);
        lcd.setCursor(0, 1);
        lcd.print(stringTwo);
        break;
      case LCD_PRESSURE_OXYGEN:
        break;
    }
  }
  
  setBarLevel(&fuel_bar, m_fuel);
  setBarLevel(&aux_bar, m_auxLevel);
  
  // Engine Power
  animatedCircularLED.setPercentage(m_enginePower);
}

void setBarLevel(Grove_LED_Bar* bar, float percent)
{
  int barLevel = round(10 * percent);
  if (barLevel < 1) { barLevel = 1; }
  bar->setLevel(barLevel);
}

void displayWarning() {
  // Check fuel, buzz if engine power up and fuel low
  if (m_warningField > 0) {
    if (!m_warningOn) {
      lcd.clear();
      lcd.setRGB(255, 0, 0);
      String stringOne = "WARNING!";
      lcd.setCursor(0, 0);
      lcd.print(stringOne);
      lcd.setCursor(0, 1);
      lcd.print(getWarningString(m_warningField));
    }

    // Flash warning LEDs
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
    setDefaultState();
  }
}

String getWarningString(byte bitField) {
  String warningString = "";
  checkAndAppend(bitField, FUEL_LOW, "Fuel Low", &warningString);
  checkAndAppend(bitField, FUEL_LINE, "Fuel Line", &warningString);
  return warningString;
}

void checkAndAppend(byte bitField, byte flag, String warning, String* message) {
  if (bitField & flag) {
    if (message->length() > 0) message->concat(" | ");
    message->concat(warning);
  }
}

void toggleLCDState () {
  m_currentLCDState++;
  if (m_currentLCDState >= LCD_NUM_STATES) m_currentLCDState = LCD_ENGINE_FUEL;
  s_lcdState = (LCDState)m_currentLCDState;
  lcd.clear();
  switch(s_lcdState) {
    case LCD_ENGINE_FUEL:
    case LCD_CHARGE_AUX:
    case LCD_PRESSURE_OXYGEN:
      break;
   default:
      lcd.setCursor(0, 0);
      lcd.print("WELCOME ABOARD");
      break;
  }
}

void updateServer() {
  unsigned long currentTime = millis();
  if(currentTime > m_lastSerialPrint + 100) {
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

boolean isPeopleDetected() {
  unsigned long currentTime = millis();
  int sensorValue = digitalRead(PIR_MOTION_SENSOR);
  if (sensorValue == HIGH) {
    m_lastPeopleActivity = currentTime;
    if (!m_powerOn) {
      m_powerOn = true;
      setDefaultState();
    }
  }
  
  if (m_lastPeopleActivity + 60000 < currentTime && m_powerOn) {
    m_powerOn = false;
    SetStatusRGB(0,0,0);
    lcd.clear();
    lcd.setRGB(0, 0, 0);
    fuel_bar.setLevel(0);
    aux_bar.setLevel(0);
    unsigned int LED_OFF[24];
    for (int i =0;i<24;i++)
    {
      LED_OFF[i]=0;
    }
    circularLED.CircularLEDWrite(LED_OFF);
  }
}
