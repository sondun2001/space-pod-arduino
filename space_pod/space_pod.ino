#include <CircularLED.h>
#include <Grove_LED_Bar.h>
#include <ChainableLED.h>
#include <rgb_lcd.h>
#include <ArduinoJson.h>
#include <AnimatedCircularLED.h>
#include <math.h>
#include <Encoder.h>

#define DEBUG

enum {
  HULL_DAMAGE = 1,
  CABIN_PRESSURE = 2,
  OXYGEN_LOW = 4,
  FUEL_LOW = 8,
  ENGINE_MALFUNCTION = 16,
  BATTERY_HEALTH = 32,
  COLLISION = 64,
  FUEL_LINE = 128,
};

// Devices
#define NUM_STATUS_LEDS  3
ChainableLED status_leds(7, 8, NUM_STATUS_LEDS);
rgb_lcd lcd;
Grove_LED_Bar fuel_bar(5, 4, 0); // Clock pin, Data pin, Orientation
Grove_LED_Bar aux_bar(9, 8, 0);
//Grove_LED_Bar water_bar(5, 4, 0);

CircularLED circularLED(6, 5);
AnimatedCircularLED animatedCircularLED(&circularLED);

const byte PIR_MOTION_SENSOR = 8;
const byte BUTTON = 3;
const int ENGINE_POWER_INPUT = A0;

// CONSTANTS
const float FLASH_DURATION = 1000;

// Resources
float m_fuel = 0.0f;
float m_auxLevel = 0.0f;
float m_waterLevel = 0.0f;
float m_oxygenLevel = 0.0f;
unsigned short m_chargeRate = 0;
unsigned short m_drainRate = 0;
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
byte m_currentWarningFlags;
bool m_warningOn = false;

enum LCDState {
  LCD_INIT = -1,
  LCD_ENGINE_FUEL,
  LCD_CHARGE_AUX,
  LCD_WATER_OXYGEN,
  LCD_NUM_STATES
};

LCDState s_lcdState = LCD_INIT;
byte m_currentLCDState = s_lcdState;

bool m_buttonOn = false;
unsigned long m_lastSerialPrint;
String m_inputString = "";

unsigned long m_lastPeopleActivity;

//Encoder m_dialEncoder(2, 3);
//long m_oldEncoderPosition  = -999;
bool m_enginePowerDirty = false;

void setup() {
  // Configure the serial communication line at 9600 baud (bits per second.)
  Serial.begin(9600);

  // Configure the angle sensor's pin for input.
  //pinMode(PIR_MOTION_SENSOR, INPUT);
  // pinMode(ENGINE_POWER_INPUT, INPUT);
  // pinMode(BUTTON, INPUT);

  fuel_bar.begin();
  aux_bar.begin();
  //water_bar.begin();

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  animatedCircularLED.setAnimationStepTime(50);
  encoder.Timer_init();

  toggleLCDState();
  setDefaultState();
}

void setDefaultState() {
  lcd.setRGB(0, 0, 255);
  SetStatusRGB(0, 180, 0);
}

void loop() {

  readSerial();
  isPeopleDetected();

  if (m_powerOn) {
    readInput();
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
      m_lastPeopleActivity = millis();
      if (m_inputString.startsWith("toggleLCD")) {
        toggleLCDState();
      } else {
        StaticJsonBuffer<200> jsonReadBuffer;
        JsonObject& jsonInput = jsonReadBuffer.parseObject(m_inputString);
        if (jsonInput.success()) {
          /*
            const char* sensor    = root["sensor"];
            long        time      = root["time"];
            double      latitude  = root["data"][0];
            double      longitude = root["data"][1];
          */
          if (jsonInput.containsKey("ep")) {
            m_enginePower = jsonInput["ep"];
          }
          if (jsonInput.containsKey("wf")) {
            m_warningField = jsonInput["wf"];
          }
          if (jsonInput.containsKey("fl")) {
            m_fuel = jsonInput["fl"];
          }
          if (jsonInput.containsKey("al")) {
            m_auxLevel = jsonInput["al"];
          }
          if (jsonInput.containsKey("wl")) {
            m_waterLevel = jsonInput["wl"];
          }
          if (jsonInput.containsKey("ol")) {
            m_oxygenLevel = jsonInput["ol"];
          }
          if (jsonInput.containsKey("cr")) {
            m_chargeRate = jsonInput["cr"];
          }
          if (jsonInput.containsKey("dr")) {
            m_drainRate = jsonInput["dr"];
          }
        }
      }
      m_inputString = "";
    }
  }
}

void readInput() {
  // Read engine power level
  /*
    int value = analogRead(ENGINE_POWER_INPUT);
    m_enginePowerInput = (float) value / 1023;
  */

  if (encoder.rotate_flag == 1)
  {
    if (encoder.direct == 1)
    {
      m_enginePowerInput += 0.02;
      if (m_enginePowerInput >= 1) m_enginePowerInput = 1;
    }
    else
    {
      m_enginePowerInput -= 0.02;
      if (m_enginePowerInput < 0) m_enginePowerInput = 0;
    }
    encoder.rotate_flag = 0;
    m_enginePowerDirty = true;
  }

  /*
    bool buttonDown = digitalRead(BUTTON);
    if (buttonDown && !m_buttonOn) {
    m_buttonOn = true;
    toggleLCDState();
    } else if (!buttonDown) {
    m_buttonOn = false;
    }
  */
}

void displayStatus() {
  if (!m_warningOn) {
    String stringOne;
    String stringTwo;
    switch (s_lcdState) {
      case LCD_ENGINE_FUEL:
        stringOne = String("ENGINE: ");
        stringTwo = String("FUEL: ");
        stringOne += (int)(m_enginePower * 100);
        stringTwo += (int)(m_fuel * 100);
        stringOne += "% ";
        stringTwo += "% ";
        break;
      case LCD_CHARGE_AUX:
        stringOne = String("CHARGE: ");
        stringTwo = String("DRAIN: ");
        stringOne += m_chargeRate;
        stringTwo += m_drainRate;
        stringOne += "W  ";
        stringTwo += "W  ";
        break;
      case LCD_WATER_OXYGEN:
        stringOne = String("WATER: ");
        stringTwo = String("OXYGEN: ");
        stringOne += (int)(m_waterLevel * 100);
        stringTwo += (int)(m_oxygenLevel * 100);
        stringOne += "%  ";
        stringTwo += "%  ";
        break;
    }
    lcd.setCursor(0, 0);
    lcd.print(stringOne);
    lcd.setCursor(0, 1);
    lcd.print(stringTwo);
  }

  setBarLevel(&fuel_bar, m_fuel);
  setBarLevel(&aux_bar, m_auxLevel);
  //setBarLevel(&water_bar, m_waterLevel);

  // Engine Power
  animatedCircularLED.setPercentage(m_enginePower);
}

void setBarLevel(Grove_LED_Bar* bar, float percent)
{
  int barLevel = round(10 * percent);
  if (barLevel < 1) {
    barLevel = 1;
  }
  bar->setLevel(barLevel);
}

void displayWarning() {
  // Check fuel, buzz if engine power up and fuel low
  if (m_warningField > 0) {
    if (!m_warningOn || m_warningField != m_currentWarningFlags) {
      lcd.clear();
      lcd.setRGB(255, 0, 0);
      String stringOne = "WARNING!";
      lcd.setCursor(0, 0);
      lcd.print(stringOne);
      lcd.setCursor(0, 1);
      lcd.print(getWarningString(m_warningField));
      m_currentWarningFlags = m_warningField;
    }

    // Flash warning LEDs
    unsigned long currentTime = millis();
    unsigned long delta = currentTime - m_lastWarnTime;
    m_flashWarnTime += delta;
    if (m_isFlashWarnOn && m_flashWarnTime < FLASH_DURATION) {
      // Process something while flash warn on
    } else if (!m_isFlashWarnOn && m_flashWarnTime < FLASH_DURATION) {
      // Process something while flash warn off
    } else {
      m_isFlashWarnOn = !m_isFlashWarnOn;
      m_flashWarnTime = 0;

      /*
      if (m_isFlashWarnOn) {
        SetStatusRGB(180, 0, 0);
      } else {
        SetStatusRGB(0, 0, 0);
      }
      */
      if (m_currentWarningFlags & BATTERY_HEALTH) {
        if (m_isFlashWarnOn) {
          status_leds.setColorRGB(0, 180, 0, 0);
        } else {
          status_leds.setColorRGB(0, 0, 0, 0);
        }
      } else {
        status_leds.setColorRGB(0, 0, 180, 0);
      }
      
      if (m_currentWarningFlags & FUEL_LOW) {
        if (m_isFlashWarnOn) {
          status_leds.setColorRGB(1, 180, 0, 0);
        } else {
          status_leds.setColorRGB(1, 0, 0, 0);
        }
      } else {
        status_leds.setColorRGB(1, 0, 180, 0);
      }
      
      if (m_currentWarningFlags & (OXYGEN_LOW | FUEL_LINE)) {
        if (m_isFlashWarnOn) {
          status_leds.setColorRGB(2, 180, 0, 0);
        } else {
          status_leds.setColorRGB(2, 0, 0, 0);
        }
      } else {
        status_leds.setColorRGB(2, 0, 180, 0);
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
  checkAndAppend(bitField, BATTERY_HEALTH, "Battery", &warningString);
  checkAndAppend(bitField, FUEL_LOW, "Fuel Low", &warningString);
  checkAndAppend(bitField, FUEL_LINE, "Fuel Line", &warningString);
  checkAndAppend(bitField, OXYGEN_LOW, "Oxygen", &warningString);
  return warningString;
}


void checkAndAppend(byte bitField, byte flag, String warning, String* message) {
  if (bitField & flag) {
    if (message->length() > 0) message->concat(" | ");
    message->concat(warning);
  }
}

void toggleLCDState () {
  if (!m_warningOn) {
    m_currentLCDState++;
    if (m_currentLCDState >= LCD_NUM_STATES) m_currentLCDState = LCD_ENGINE_FUEL;
    s_lcdState = (LCDState)m_currentLCDState;
    lcd.clear();
    switch (s_lcdState) {
      case LCD_ENGINE_FUEL:
      case LCD_CHARGE_AUX:
      case LCD_WATER_OXYGEN:
        break;
      default:
        lcd.setCursor(0, 0);
        lcd.print("WELCOME ABOARD");
        break;
    }
  }
}

void updateServer() {
  unsigned long currentTime = millis();
  if (currentTime > m_lastSerialPrint + 100) {
    /*
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.createObject();
      root["epi"] = m_enginePowerInput;
      root.printTo(Serial);
    */

    if (m_enginePowerDirty) {
      String jsonString = "{\"epi\":\"";
      jsonString += m_enginePowerInput;
      jsonString += "\"}";

      // print it:
      Serial.println(jsonString);

      m_enginePowerDirty = false;
    }

    m_lastSerialPrint = millis();
  }
}

void SetStatusRGB(int r, int g, int b) {
  for (int i = 0; i < NUM_STATUS_LEDS; i++) {
    status_leds.setColorRGB(i, r, g, b);
  }
}

boolean isPeopleDetected() {
  unsigned long currentTime = millis();
  // int sensorValue = digitalRead(PIR_MOTION_SENSOR);
  // bool buttonDown = digitalRead(BUTTON);
  bool buttonDown = false;
  if (/*sensorValue == HIGH || */buttonDown) {
    m_lastPeopleActivity = currentTime;
    if (!m_powerOn) {
      m_powerOn = true;
      setDefaultState();
    }
  }

  if (m_lastPeopleActivity + 60000 * 5 < currentTime && m_powerOn) {
    m_powerOn = false;
    SetStatusRGB(0, 0, 0);
    lcd.clear();
    lcd.setRGB(0, 0, 0);
    fuel_bar.setLevel(0);
    aux_bar.setLevel(0);
    //water_bar.setLevel(0);
    animatedCircularLED.setPercentage(0);
    m_currentWarningFlags = 0;
  }
}
