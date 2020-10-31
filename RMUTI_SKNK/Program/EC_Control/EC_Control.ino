#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "REG_Meter.h"
#include <ModbusMaster.h>
#include <HardwareSerial.h>
#include "Adafruit_MCP23008.h"

//const char *hostname = "Fertilizer-Control";

//************* PIN Set *****************//

#define dosingA       0
#define dosingB       1
#define pumpMix       2
#define pumpWater     3

#define menuPIN       36     //Menu by Analog Read

#define AUTO   0
#define TIMER  1

int adc_key     = 0;
int adc_key_in  = 0;
#define btnUP     0
#define btnDOWN   1
#define btnENTER  2
#define btnBACK   3
#define btnNONE   4

int Mode = AUTO;

int EC          = 0.0;

int ecLOW       = 0;
int ecHIGH      = 0;

int timeA       = 0;
int timeB       = 0;
int timeMix     = 0;

int delayActive = 0;

int calEC = 0;

int SetHourOn = 0;
int SetMinuteOn = 0;

unsigned int addMode = 95;
unsigned int addecLOW    = 100;
unsigned int addecHIGH   = 105;

unsigned int addtimeA    = 120;
unsigned int addtimeB    = 125;
unsigned int addtimeMix     = 140;

unsigned int adddelayActive = 145;

unsigned int addcalEC = 150;

unsigned int addSetHourOn = 155;
unsigned int addSetMinuteOn = 160;

unsigned long currentMillis = 0;

boolean preStateEC          = 0;
unsigned long preMillisEC   = 0;

boolean stateA = 0;
unsigned long timeACount = 0;
unsigned long timeACountStart = 0;

boolean stateB = 0;
unsigned long timeBCount = 0;
unsigned long timeBCountStart = 0;

boolean stateMix = 0;
unsigned long timeMixCount = 0;
unsigned long timeMixCountStart = 0;

boolean stateMixA = 0;
boolean stateMixB = 0;

unsigned long previousTime = 0;
int Show = 0;

static long startPress = 0;

Adafruit_MCP23008 mcp;
LiquidCrystal_I2C lcd(0x27, 16, 2);
HardwareSerial MySerial(2);
ModbusMaster node;

void readSensor()
{
  lcd.setCursor(0, 0);
  lcd.print("ReadSensor RS485");
  lcd.setCursor(0, 1);
  lcd.print("    Wait....    ");

  GET_METER();

  EC = (DATA_METER_EC[1] / 10);
  EC = EC + calEC;
}

void setup()
{
  mcp.begin();

  mcp.pinMode(dosingA, OUTPUT);      mcp.digitalWrite(dosingA, LOW);
  mcp.pinMode(dosingB, OUTPUT);      mcp.digitalWrite(dosingB, LOW);
  mcp.pinMode(pumpMix, OUTPUT);      mcp.digitalWrite(pumpMix, LOW);
  mcp.pinMode(pumpWater, OUTPUT);    mcp.digitalWrite(pumpWater, LOW);


  Serial.begin(9600);
  MySerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println();
  delay(200);

  EEPROM.begin( 512 );
  //EEPROM.get(0, sett);

  ecLOW       = EEPROM.readInt(addecLOW);
  ecHIGH      = EEPROM.readInt(addecHIGH);
  timeA       = EEPROM.readInt(addtimeA);
  timeB       = EEPROM.readInt(addtimeB);
  timeMix     = EEPROM.readInt(addtimeMix);
  delayActive = EEPROM.readInt(adddelayActive);
  calEC    = EEPROM.readInt(addcalEC);
  SetHourOn    = EEPROM.readInt(addSetHourOn);
  SetMinuteOn    = EEPROM.readInt(addSetMinuteOn);

  Mode = EEPROM.readInt(addMode);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("   Fertilizer   ");
  lcd.setCursor(0, 1);
  lcd.print("     Control    ");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print(" ThingControl.io");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("   Strat ....   ");
  delay(2000);
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("    Scan WiFi   ");
  lcd.setCursor(0, 1);
  lcd.print("      ....      ");


  lcd.setCursor(0, 0);
  lcd.print(" Connect Finish ");
  lcd.setCursor(0, 1);
  lcd.print("  !! Online !!  ");
  delay(2000);
  lcd.clear();

  previousTime = millis();
}

void loop()
{
  adc_key = read_ADC_buttons();

  switch (adc_key)
  {
    case btnENTER:
      {
        lcd.clear();
        settingMenu();
        break;
      }
  }

  if (previousTime > millis())
  {
    previousTime = 0;
  }

  if (millis() - previousTime > 4000)
  {
    previousTime = millis();
    Show++;
    lcd.clear();
    if (Show > 2) {
      Show = 0;
    }
  }


  switch (Mode)
  {
    case AUTO:
      {
        switch (Show)
        {
          case 0:
            {
              Show1();
              break;
            }
          case 1:
            {
              Show2();
              break;
            }
          case 2:
            {
              Show3();
              break;
            }
        }

        ///////////////////////////////////////////////

        currentMillis = millis() / 1000;

        // Process EC
        if (EC < ecLOW)
        {
          if (preStateEC == 0)
          {
            preStateEC = 1;
            preMillisEC = currentMillis;
          }
          if ((EC < ecLOW) && (currentMillis - preMillisEC > delayActive))
          {
            // Active Output
            stateA = 1;
            stateB = 1;
          }
        } else if (EC >= ecHIGH)
        {
          // Deactive Output
          stateA = 0;
          stateB = 0;
          preStateEC = 0;
          //stateMixEC = 0;
        }

        ////////////////////////////////////////////////

        if (timeACountStart > millis())
        {
          timeACountStart = 0;
        }
        if ((timeACount < millis() / 1000) && (stateMix == 0))
        {
          if (stateA == 1)
          {
            timeACountStart++;
          }
        }
        timeACount = millis() / 1000;

        ////////////////////////////////////////////////

        if (timeBCountStart > millis())
        {
          timeBCountStart = 0;
        }
        if ((timeBCount < millis() / 1000) && (stateMix == 0))
        {
          if (stateB == 1)
          {
            timeBCountStart++;
          }
        }
        timeBCount = millis() / 1000;

        ////////////////////////////////////////////////

        if (timeMixCountStart > millis())
        {
          timeMixCountStart = 0;
        }
        if ((timeMixCount < millis() / 1000) && (stateMix == 1))
        {
          timeMixCountStart++;
        }
        if (timeMixCountStart > timeMix)
        {
          stateMix = 0;
          stateMixA = 0;
          stateMixB = 0;

          timeACountStart = 0;
          timeBCountStart = 0;
          timeMixCountStart = 0;
        }
        timeMixCount = millis() / 1000;

        ////////////////////////////////////////////////

        if (stateA == 1)                        // สั่งปั้มปุ๋ย A ทำงาน
        {
          if (timeACountStart < timeA)
          {
            mcp.digitalWrite(dosingA, HIGH);
          }
          else if (timeACountStart >= timeA)
          {
            stateMixA = 1;
            mcp.digitalWrite(dosingA, LOW);
          }
          else
          {
            stateMixA = 1;
          }
        }
        else
        {
          mcp.digitalWrite(dosingA, LOW);
        }

        ////////////////////////////////////////////////

        if (stateB == 1)                        // สั่งปั้มปุ๋ย B ทำงาน
        {
          if (timeBCountStart < timeB)
          {
            mcp.digitalWrite(dosingB, HIGH);
          }
          else if (timeBCountStart >= timeB)
          {
            stateMixB = 1;
            mcp.digitalWrite(dosingB, LOW);
          }
          else
          {
            stateMixB = 1;
          }
        }
        else
        {
          mcp.digitalWrite(dosingB, LOW);
        }

        ////////////////////////////////////////////////

        if ((stateA == 1) && (stateB == 1))
        {
          if ((stateMixA == 1) && (stateMixB == 1))
          {
            stateMix = 1;
          }
        }


        if (stateMix == 1)                    // สั่งปั้มผสมน้ำทำงาน
        {
          mcp.digitalWrite(pumpMix, HIGH);
        }
        else
        {
          mcp.digitalWrite(pumpMix, LOW);
        }

        ////////////////////////////////////////////////
        break;
      }

    case TIMER:
      {
        lcd.setCursor(0, 0);
        lcd.print("     Manual     ");
        lcd.setCursor(0, 1);
        lcd.print("                ");
        break;
      }
  }
  /////////////////////////////////////////////////////////////////////

} // End void loop()

int read_ADC_buttons()
{
  adc_key_in = analogRead(menuPIN);

  if (adc_key_in > 2000)
  {
    return btnNONE;
  }
  if (adc_key_in < 50)
  {
    delay(250);
    return btnUP;
  }
  if ((adc_key_in < 600) && (adc_key_in > 200))
  {
    delay(250);
    return btnDOWN;
  }
  if ((adc_key_in < 1400) && (adc_key_in > 700))
  {
    delay(250);
    return btnENTER;
  }
  if ((adc_key_in < 2000) && (adc_key_in > 1500))
  {
    delay(250);
    return btnBACK;
  }
  return btnNONE;
}

void Show1()
{
  lcd.setCursor(0, 0);
  lcd.print("EC: ");
  if (EC < 10) {
    lcd.print("000");
  }
  else if (EC < 100) {
    lcd.print("00");
  }
  else if (EC < 1000) {
    lcd.print("0");
  }
  lcd.print(EC);
  lcd.print(" uS/cm  ");
  lcd.setCursor(0, 1);
  lcd.print("setEC: ");
  if (ecLOW < 10) {
    lcd.print("000");
  }
  else if (ecLOW < 100) {
    lcd.print("00");
  }
  else if (ecLOW < 1000) {
    lcd.print("0");
  }
  lcd.print(ecLOW);
  lcd.print("-");
  if (ecHIGH < 10) {
    lcd.print("000");
  }
  else if (ecHIGH < 100) {
    lcd.print("00");
  }
  else if (ecHIGH < 1000) {
    lcd.print("0");
  }
  lcd.print(ecHIGH);
}

void Show2()
{

}

void Show3()
{

}


void GET_METER()
{ // Update read all data
  delay(100);
  for (int i = 0; i < Total_of_Reg_EC ; i++)
  {
    DATA_METER_EC [i] = Read_Meter(ECID_meter, Reg_addr_EC[i]);
  }
}


long Read_Meter(char addr , int  REG)
{
  uint8_t result;
  int data = 0;

  node.begin(addr, MySerial);

  result = node.readHoldingRegisters (REG, 1); ///< Modbus function 0x03 Read Holding Registers

  delay(100);
  if (result == node.ku8MBSuccess)
  {
    data = node.getResponseBuffer(0);

    //Serial.println("Connec modbus Ok.");
    return data;
  } else
  {
    Serial.print("Connec modbus fail. REG >>> "); Serial.println(REG); // Debug
    delay(100);
    return 0;
  }
}

void settingMenu()
{
  mcp.digitalWrite(dosingA, LOW);
  mcp.digitalWrite(dosingB, LOW);
  mcp.digitalWrite(pumpMix, LOW);

  stateMix = 0;
  stateMixA = 0;
  stateMixB = 0;
  timeACountStart = 0;
  timeBCountStart = 0;
  timeMixCountStart = 0;

  unsigned int varMenu = 1;
  char* showMenu[] = {"<1.Set EC Low  >",      // myMenu[0]  1
                      "<2.Set EC High >",      // myMenu[1]  2
                      "<3.Set Delay   >",      // myMenu[2]  3
                      "<4.Set TimeFerA>",      // myMenu[3]  4
                      "<5.Set TimeFerB>",      // myMenu[4]  5
                      "<6.Set TimeMix >",      // myMenu[5]  6
                      "<7.Set Cal. EC >",      // myMenu[6]  7
                      "<8.Set Mode    >",      // myMenu[7]  8
                      "<9.Set M.Timer >",      // myMenu[8]  9
                      "<10.Test OUTPUT>"
                     };     // myMenu[9]  10
  int i = 1;

  int outPIN[4] = {dosingA, dosingB, pumpMix, pumpWater};
  int pin = 0;
  int Status[4] = {LOW, LOW, LOW, LOW};

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("      Menu      ");
  lcd.setCursor(0, 1);
  lcd.print("    Setting.");
  delay(200);
  lcd.print(".");
  delay(400);
  lcd.print(".");
  delay(600);
  lcd.print(".");
  delay(500);
  lcd.clear();

  while (i == 1)
  {
    lcd.setCursor(0, 0);
    switch (varMenu)
    {
      case 1:
        {
          lcd.print(showMenu[0]);
          lcd.setCursor(0, 1);
          if (ecLOW < 10) {
            lcd.print("000");
          }
          else if (ecLOW < 100) {
            lcd.print("00");
          }
          else if (ecLOW < 1000) {
            lcd.print("0");
          }
          lcd.print(ecLOW);
          lcd.print(" ");
          lcd.print("uS/cm ");
          lcd.setCursor(12, 1);
          lcd.print(" SET");
          break;
        }
      case 11:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[0]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (ecLOW < 10) {
              lcd.print("000");
            }
            else if (ecLOW < 100) {
              lcd.print("00");
            }
            else if (ecLOW < 1000) {
              lcd.print("0");
            }
            lcd.print(ecLOW);
          }
          else
          {
            lcd.setCursor(0, 1);
            lcd.print("    ");
          }
          lcd.print(" ");
          lcd.print("uS/cm ");
          break;
        }

      case 2:
        {
          lcd.print(showMenu[1]);
          lcd.setCursor(0, 1);
          if (ecHIGH < 10) {
            lcd.print("000");
          }
          else if (ecHIGH < 100) {
            lcd.print("00");
          }
          else if (ecHIGH < 1000) {
            lcd.print("0");
          }
          lcd.print(ecHIGH);
          lcd.print(" ");
          lcd.print("uS/cm ");
          lcd.setCursor(12, 1);
          lcd.print(" SET");
          break;
        }
      case 21:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[1]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (ecHIGH < 10) {
              lcd.print("000");
            }
            else if (ecHIGH < 100) {
              lcd.print("00");
            }
            else if (ecHIGH < 1000) {
              lcd.print("0");
            }
            lcd.print(ecHIGH);
          }
          else
          {
            lcd.setCursor(0, 1);
            lcd.print("    ");
          }
          lcd.print(" ");
          lcd.print("uS/cm ");
          break;
        }

      case 3:
        {
          lcd.print(showMenu[2]);
          lcd.setCursor(0, 1);
          if (delayActive < 10) {
            lcd.print("0");
          }
          lcd.print(delayActive);
          lcd.print(" ");
          lcd.print("Sec.    ");
          lcd.setCursor(12, 1);
          lcd.print(" SET");
          break;
        }
      case 31:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[2]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (delayActive < 10) {
              lcd.print("0");
            }
            lcd.print(delayActive);
          }
          else
          {
            lcd.setCursor(0, 1);
            lcd.print("  ");
          }
          lcd.print(" ");
          lcd.print("Sec.    ");
          break;
        }

      case 4:
        {
          lcd.print(showMenu[3]);
          lcd.setCursor(0, 1);
          if (timeA < 10) {
            lcd.print("00");
          }
          else if (timeA < 100) {
            lcd.print("0");
          }
          lcd.print(timeA);
          lcd.print(" ");
          lcd.print("Sec.    ");
          lcd.setCursor(12, 1);
          lcd.print(" SET");
          break;
        }
      case 41:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[3]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (timeA < 10) {
              lcd.print("00");
            }
            else if (timeA < 100) {
              lcd.print("0");
            }
            lcd.print(timeA);
          }
          else
          {
            lcd.setCursor(0, 1);
            lcd.print("   ");
          }
          lcd.print(" ");
          lcd.print("Sec.    ");
          break;
        }

      case 5:
        {
          lcd.print(showMenu[4]);
          lcd.setCursor(0, 1);
          if (timeB < 10) {
            lcd.print("00");
          }
          else if (timeB < 100) {
            lcd.print("0");
          }
          lcd.print(timeB);
          lcd.print(" ");
          lcd.print("Sec.    ");
          lcd.setCursor(12, 1);
          lcd.print(" SET");
          break;
        }
      case 51:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[4]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (timeB < 10) {
              lcd.print("00");
            }
            else if (timeB < 100) {
              lcd.print("0");
            }
            lcd.print(timeB);
          }
          else
          {
            lcd.setCursor(0, 1);
            lcd.print("   ");
          }
          lcd.print(" ");
          lcd.print("Sec.    ");
          break;
        }

      case 6:
        {
          lcd.print(showMenu[5]);
          lcd.setCursor(0, 1);
          if (timeMix < 10) {
            lcd.print("00");
          }
          else if (timeMix < 100) {
            lcd.print("0");
          }
          lcd.print(timeMix);
          lcd.print(" ");
          lcd.print("Sec.    ");
          lcd.setCursor(12, 1);
          lcd.print(" SET");
          break;
        }
      case 61:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[5]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (timeMix < 10) {
              lcd.print("00");
            }
            else if (timeMix < 100) {
              lcd.print("0");
            }
            lcd.print(timeMix);
          }
          else
          {
            lcd.setCursor(0, 1);
            lcd.print("   ");
          }
          lcd.print(" ");
          lcd.print("Sec.    ");
          break;
        }

      case 7:
        {
          lcd.print(showMenu[6]);
          lcd.setCursor(0, 1);
          if (calEC < 10) {
            lcd.print("000");
          }
          else if (calEC < 100) {
            lcd.print("00");
          }
          else if (calEC < 1000) {
            lcd.print("0");
          }
          lcd.print(calEC);
          lcd.print(" ");
          lcd.print("uS/cm   ");
          lcd.setCursor(12, 1);
          lcd.print(" SET");
          break;
        }
      case 71:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[6]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (calEC < 10) {
              lcd.print("000");
            }
            else if (calEC < 100) {
              lcd.print("00");
            }
            else if (calEC < 1000) {
              lcd.print("0");
            }
            lcd.print(calEC);
          }
          else
          {
            lcd.setCursor(0, 1);
            lcd.print("    ");
          }
          lcd.print(" ");
          lcd.print("uS/cm   ");
          break;
        }

      case 8:
        {
          lcd.print(showMenu[7]);
          lcd.setCursor(0, 1);
          switch (Mode)
          {
            case TIMER:
              {
                lcd.setCursor(0, 1);
                lcd.print("-Mode : TIMER   ");
                break;
              }
            case AUTO:
              {
                lcd.setCursor(0, 1);
                lcd.print("-Mode : AUTO    ");
                break;
              }
          }
          break;
        }
      case 81:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[7]);
          if ((j % 2) == 0)
          {
            switch (Mode)
            {
              case TIMER:
                {
                  lcd.setCursor(0, 1);
                  lcd.print("-Mode : TIMER   ");
                  break;
                }
              case AUTO:
                {
                  lcd.setCursor(0, 1);
                  lcd.print("-Mode : AUTO    ");
                  break;
                }
            }
          }
          else
          {
            lcd.setCursor(8, 1);
            lcd.print("     ");
          }
          break;
        }

      case 9:
        {
          lcd.print(showMenu[8]);
          lcd.setCursor(0, 1);
          if (SetHourOn < 10) {
            lcd.print("0");
          }
          lcd.print(SetHourOn);
          lcd.print(":");
          if (SetMinuteOn < 10) {
            lcd.print("0");
          }
          lcd.print(SetMinuteOn);
          lcd.print("       ");
          lcd.setCursor(12, 1);
          lcd.print(" SET");
          break;
        }
      case 91:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[8]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (SetHourOn < 10) {
              lcd.print("0");
            }
            lcd.print(SetHourOn);
            lcd.print(":");
            if (SetMinuteOn < 10) {
              lcd.print("0");
            }
            lcd.print(SetMinuteOn);
          }
          else
          {
            lcd.setCursor(0, 1);
            lcd.print("  ");
          }
          break;
        }

      case 92:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[8]);
          if ((j % 2) == 0)
          {
            lcd.setCursor(0, 1);
            if (SetHourOn < 10) {
              lcd.print("0");
            }
            lcd.print(SetHourOn);
            lcd.print(":");
            if (SetMinuteOn < 10) {
              lcd.print("0");
            }
            lcd.print(SetMinuteOn);
          }
          else
          {
            lcd.setCursor(3, 1);
            lcd.print("  ");
          }
          lcd.print("          ");
          break;
        }

      case 10:
        {
          lcd.print(showMenu[9]);
          lcd.setCursor(0, 1);
          lcd.print("                ");
          break;
        }
      case 101:
        {
          unsigned long j = millis() / 390;
          lcd.print(showMenu[9]);
          lcd.setCursor(0, 1);
          lcd.print("Test Ch: ");
          lcd.print(pin + 1);
          mcp.digitalWrite(outPIN[pin], Status[pin]);
          lcd.print(" ");
          lcd.print(": ");
          switch (Status[pin])
          {
            case LOW:
              {
                if ((j % 2) == 0)
                {
                  lcd.setCursor(13, 1);
                  lcd.print("OFF");
                } else
                {
                  lcd.setCursor(13, 1);
                  lcd.print("   ");
                }
                break;
              }
            case HIGH:
              {
                if ((j % 2) == 0)
                {
                  lcd.setCursor(13, 1);
                  lcd.print(" ON");
                } else
                {
                  lcd.setCursor(13, 1);
                  lcd.print("   ");
                }
                break;
              }
          }
          break;
        }
    }

    adc_key = read_ADC_buttons();    //อ่านปุ่มกด ใน switch case

    switch (adc_key)
    {
      case btnBACK:
        {
          if (varMenu == 101)
          {
            mcp.digitalWrite(dosingA, LOW);
            mcp.digitalWrite(dosingB, LOW);
            mcp.digitalWrite(pumpWater, LOW);
            mcp.digitalWrite(pumpMix, LOW);
            varMenu = 10;
            lcd.clear();
            break;
          }
          if (varMenu == 10)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }

          if (varMenu == 92)
          {
            SetMinuteOn = EEPROM.readInt(addSetMinuteOn);
            varMenu = 91;
            lcd.clear();
            break;
          }
          if (varMenu == 91)
          {
            SetHourOn = EEPROM.readInt(addSetHourOn);
            varMenu = 9;
            lcd.clear();
            break;
          }
          if (varMenu == 9)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }

          if (varMenu == 81)
          {
            Mode = EEPROM.readInt(addMode);
            varMenu = 8;
            lcd.clear();
            break;
          }
          if (varMenu == 8)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }

          if (varMenu == 71)
          {
            calEC = EEPROM.readInt(addcalEC);
            varMenu = 7;
            lcd.clear();
            break;
          }
          if (varMenu == 7)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }

          if (varMenu == 61)
          {
            timeMix = EEPROM.readInt(addtimeMix);
            varMenu = 6;
            lcd.clear();
            break;
          }
          if (varMenu == 6)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }

          if (varMenu == 51)
          {
            timeB = EEPROM.readInt(addtimeB);
            varMenu = 5;
            lcd.clear();
            break;
          }
          if (varMenu == 5)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }

          if (varMenu == 41)
          {
            timeA = EEPROM.readInt(addtimeA);
            varMenu = 4;
            lcd.clear();
            break;
          }
          if (varMenu == 4)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }

          if (varMenu == 31)
          {
            delayActive = EEPROM.readInt(adddelayActive);
            varMenu = 3;
            lcd.clear();
            break;
          }
          if (varMenu == 3)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }

          if (varMenu == 21)
          {
            ecHIGH = EEPROM.readInt(addecHIGH);
            varMenu = 2;
            lcd.clear();
            break;
          }
          if (varMenu == 2)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;

            break;
          }

          if (varMenu == 11)
          {
            ecLOW = EEPROM.readInt(addecLOW);
            varMenu = 1;
            lcd.clear();
            break;
          }
          if (varMenu == 1)
          {
            i = 0;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  CYCLE START  ");
            lcd.setCursor(0, 1);
            lcd.print("    Wait.");
            delay(200);
            lcd.print(".");
            delay(400);
            lcd.print(".");
            delay(600);
            lcd.print(".    ");
            delay(500);
            lcd.clear();
            break;
          }
          break;
        }

      case btnUP:
        {
          if (varMenu == 101)
          {
            pin++;
            if (pin > 3) pin = 3;
            break;
          }
          if (varMenu == 10)
          {
            varMenu++;
            if (varMenu > 10) varMenu = 10;
            break;
          }

          if (varMenu == 92)
          {
            SetMinuteOn++;
            if (SetMinuteOn > 59) SetMinuteOn = 0;
            break;
          }
          if (varMenu == 91)
          {
            SetHourOn++;
            if (SetHourOn > 23) SetHourOn = 0;
            break;
          }
          if (varMenu == 9)
          {
            varMenu++;
            break;
          }

          if (varMenu == 81)
          {
            if (Mode == AUTO)
            {
              Mode = TIMER;
            } else
            {
              Mode = AUTO;
            }
            break;
          }
          if (varMenu == 8)
          {
            varMenu++;
            break;
          }

          if (varMenu == 71)
          {
            calEC++;
            if (calEC > 4400) calEC = 4400;
            break;
          }
          if (varMenu == 7)
          {
            varMenu++;
            break;
          }

          if (varMenu == 61)
          {
            timeMix++;
            if (timeMix > 900) timeMix = 1;
            break;
          }
          if (varMenu == 6)
          {
            varMenu++;
            break;
          }

          if (varMenu == 51)
          {
            timeB++;
            if (timeB > 900) timeB = 1;
            break;
          }
          if (varMenu == 5)
          {
            varMenu++;
            break;
          }

          if (varMenu == 41)
          {
            timeA++;
            if (timeA > 900) timeA = 1;
            break;
          }
          if (varMenu == 4)
          {
            varMenu++;
            break;
          }

          if (varMenu == 31)
          {
            delayActive++;
            if (delayActive > 900) delayActive = 1;
            break;
          }
          if (varMenu == 3)
          {
            varMenu++;
            break;
          }

          if (varMenu == 21)
          {
            ecHIGH++;
            if (ecHIGH > 4400) ecHIGH = 1;
            break;
          }
          if (varMenu == 2)
          {
            varMenu++;
            break;
          }

          if (varMenu == 11)
          {
            ecLOW++;
            if (ecLOW > ecHIGH - 2) ecLOW = ecHIGH - 2;
            break;
          }
          if (varMenu == 1)
          {
            varMenu++;
            break;
          }
          break;
        }

      case btnDOWN:
        {
          if (varMenu == 101)
          {
            pin--;
            if (pin < 0) pin = 0;
            break;
          }
          if (varMenu == 10)
          {
            varMenu--;
            break;
          }

          if (varMenu == 92)
          {
            SetMinuteOn--;
            if (SetMinuteOn < 0) SetMinuteOn = 59;
            break;
          }
          if (varMenu == 91)
          {
            SetHourOn--;
            if (SetHourOn < 0) SetHourOn = 23;
            break;
          }
          if (varMenu == 9)
          {
            varMenu--;
            break;
          }

          if (varMenu == 81)
          {
            if (Mode == AUTO)
            {
              Mode = TIMER;
            } else
            {
              Mode = AUTO;
            }
            break;
          }
          if (varMenu == 8)
          {
            varMenu--;
            break;
          }

          if (varMenu == 71)
          {
            calEC--;
            break;
          }
          if (varMenu == 7)
          {
            varMenu--;
            break;
          }

          if (varMenu == 61)
          {
            timeMix--;
            if (timeMix < 1) timeMix = 900;
            break;
          }
          if (varMenu == 6)
          {
            varMenu--;
            break;
          }

          if (varMenu == 51)
          {
            timeB--;
            if (timeB < 1) timeB = 900;
            break;
          }
          if (varMenu == 5)
          {
            varMenu--;
            break;
          }

          if (varMenu == 41)
          {
            timeA--;
            if (timeA < 1) timeA = 900;
            break;
          }
          if (varMenu == 4)
          {
            varMenu--;
            break;
          }

          if (varMenu == 31)
          {
            delayActive--;
            if (delayActive < 1) delayActive = 900;
            break;
          }
          if (varMenu == 3)
          {
            varMenu--;
            break;
          }

          if (varMenu == 21)
          {
            ecHIGH--;
            if (ecHIGH < ecLOW + 2) ecHIGH = ecLOW + 2;
            break;
          }
          if (varMenu == 2)
          {
            varMenu--;
            break;
          }

          if (varMenu == 11)
          {
            ecLOW--;
            if (ecLOW < 0) ecLOW = 0;
            break;
          }
          if (varMenu == 1)
          {
            varMenu--;
            if (varMenu < 1) varMenu = 1;
            break;
          }
          break;
        }

      case btnENTER:
        {
          if (varMenu == 101)
          {
            if (Status[pin] == LOW)
            {
              Status[pin] = HIGH;
            } else
            {
              Status[pin] = LOW;
            }
            break;
          }
          if (varMenu == 10)
          {
            varMenu = 101;
            lcd.clear();
            break;
          }

          if (varMenu == 92)
          {
            EEPROM.writeInt(addSetMinuteOn, SetMinuteOn);
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 9;
            lcd.clear();
            break;
          }
          if (varMenu == 91)
          {
            EEPROM.writeInt(addSetHourOn, SetHourOn);
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 92;
            lcd.clear();
            break;
          }
          if (varMenu == 9)
          {
            varMenu = 91;
            lcd.clear();
            break;
          }

          if (varMenu == 81)
          {
            EEPROM.writeInt(addMode, Mode);
            EEPROM.commit();
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 8;
            lcd.clear();
            break;
          }
          if (varMenu == 8)
          {
            varMenu = 81;
            lcd.clear();
            break;
          }

          if (varMenu == 71)
          {
            EEPROM.writeInt(addcalEC, calEC);
            EEPROM.commit();
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 7;
            lcd.clear();
            break;
          }
          if (varMenu == 7)
          {
            varMenu = 71;
            lcd.clear();
            break;
          }

          if (varMenu == 61)
          {
            EEPROM.writeInt(addtimeMix, timeMix);
            EEPROM.commit();
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 6;
            lcd.clear();
            break;
          }
          if (varMenu == 6)
          {
            varMenu = 61;
            lcd.clear();
            break;
          }

          if (varMenu == 51)
          {
            EEPROM.writeInt(addtimeB, timeB);
            EEPROM.commit();
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 5;
            lcd.clear();
            break;
          }
          if (varMenu == 5)
          {
            varMenu = 51;
            lcd.clear();
            break;
          }

          if (varMenu == 41)
          {
            EEPROM.writeInt(addtimeA, timeA);
            EEPROM.commit();
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 4;
            lcd.clear();
            break;
          }
          if (varMenu == 4)
          {
            varMenu = 41;
            lcd.clear();
            break;
          }

          if (varMenu == 31)
          {
            EEPROM.writeInt(adddelayActive, delayActive);
            EEPROM.commit();
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 3;
            lcd.clear();
            break;
          }
          if (varMenu == 3)
          {
            varMenu = 31;
            lcd.clear();
            break;
          }

          if (varMenu == 21)
          {
            EEPROM.writeInt(addecHIGH, ecHIGH);
            EEPROM.commit();
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 2;
            lcd.clear();
            break;
          }
          if (varMenu == 2)
          {
            varMenu = 21;
            lcd.clear();
            break;
          }

          if (varMenu == 11)
          {
            EEPROM.writeInt(addecLOW, ecLOW);
            EEPROM.commit();
            lcd.setCursor(12, 1);
            lcd.print("SAVE");
            delay(200);
            varMenu = 1;
            lcd.clear();
            break;
          }
          if (varMenu == 1)
          {
            varMenu = 11;
            lcd.clear();
            break;
          }
          break;
        }
    }// End switch(adc_key)
  }// End while(i == 1)
}// Ens settingMenu()
