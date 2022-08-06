#include <Arduino.h>

#include <C61JCAM/Logging.hpp>

#include <C61JCAM/COMCONFIG.h>
#include <C61JCAM/PICON.h>
#include <C61JCAM/PWMS.hpp>

PWM led;
PWM buz;

LOGGING logging;

void setup()
{
  // setup buz power
  pinMode(HIGH_VOLTAGE_SW, OUTPUT);
  digitalWrite(HIGH_VOLTAGE_SW, LOW);

  led.PWMInit(2, 1, 8, 2, 127);
  buz.PWMInit(0, 2000, 8, BUZ_SW, 0);

  PIPinsInit();

  // init serials
  Serial.begin(115200);
  Serial.println("ESP launched");
  Serial2.begin(115200, SERIAL_8N1, COMBOARDRX, COMBOARDTX);

  logging.initSPI();
}

void loop()
{
  while (Serial2.available())
  {
    char cmdFromComBoard = Serial2.read();
    switch (cmdFromComBoard)
    {
    case CHECKSENSORCMD:
    {
      int sensorStatus = 0; // 0:ok 1:not ok
      sensorStatus = logging.checkSensor();
      led.PWMChangeFreq(20);
      PILaunch();
      PIRECStart();
      delay(60000);
      sensorStatus = ISPICAMOK();
      if (sensorStatus)
      {
        Serial2.print(WRONGSENSORCMD);
      }
      else
      {
        Serial2.print(CHECKSENSORCMD);
      }
      delay(1000);
      PIRECStopAndKill();
      led.PWMChangeFreq(1);
      break;
    }

    case STARTLOGGINGCMD:
    {
      Serial2.print(STARTLOGGINGCMD);
      led.PWMChangeFreq(20);
      PILaunch();
      logging.eraseFlash();
      PIRECStart();
      logging.logTaskCreate();
      led.PWMChangeFreq(10);
      break;
    }

    case STOPLOGGINGCMD:
    {
      Serial2.print(STOPLOGGINGCMD);
      led.PWMChangeFreq(5);
      logging.logTaskDelete();
      PIRECStopAndKill();
      led.PWMChangeFreq(1);
      break;
    }

    case DATAERACECMD:
    {
      Serial2.print(DATAERACECMD);
      led.PWMChangeFreq(20);
      logging.eraseFlash();
      led.PWMChangeFreq(1);
      break;
    }

    case ENABLEBUZCMD:
    {
      Serial2.print(ENABLEBUZCMD);
      // buzzer on
      break;
    }

    case DISABLEBUZCMD:
    {
      Serial2.print(DISABLEBUZCMD);
      // buzzer off
      break;
    }
    }
  }

  while (Serial.available())
  {
    unsigned char cmdFromPC = Serial.read();
    Serial.print("recieved cmd from Serial0 is '");
    Serial.print(cmdFromPC);
    Serial.println("'");

    switch (cmdFromPC)
    {
    case READMODECMD:
      logging.readAllFlash();
      break;
    }
  }

  delay(1000);
}