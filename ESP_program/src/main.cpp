#include <Arduino.h>

#include <C61JCAM/SPICONFIG.h>

#include <C61JCAM/COMCONFIG.h>
#include <C61JCAM/PICON.h>
#include <C61JCAM/PWMS.hpp>

// PWM led;
PWM buz;

SPICREATE::SPICreate SPIC1;
MPU mpu9250;
Flash flash1;
LPS lps22hb;

namespace LOGGING
{
  uint8_t isAttachingData = 0;
  uint32_t latestDatasetIndex = 0;
  uint32_t latestFlashPage = 0;
  uint8_t lps_counter = 0;
  uint8_t isFlashErased = 0;   // 1:Erased 0:not Erased
  uint8_t isLoggingGoing = 0;  // 1:Going 0:not Going
  uint8_t isCheckedSensor = 0; // 1:Checked 0:not Checked
  char sensorStatus;

  uint8_t latestDataset[256];
  TaskHandle_t LoggingHandle;
};

void initVariables()
{
  LOGGING::isAttachingData = 0;
  LOGGING::latestDatasetIndex = 0;
  LOGGING::latestFlashPage = 0;
  LOGGING::lps_counter = 0;
}

int checkSensor()
{
  Serial.print("LPS:");
  Serial.println(lps22hb.WhoImI());
  Serial.print("MPU:");
  Serial.println(mpu9250.WhoImI());
  //(lps22hb.WhoImI() == 177) &&
  if ((mpu9250.WhoImI() == 113))
  {
    return 0;
  }
  return 1;
}

void eraseFlash()
{
  if (!LOGGING::isFlashErased)
  {
    flash1.erase();
  }
  LOGGING::isFlashErased = 1;
}

void readAllFlash()
{
  for (int j = 0; j < 65536; j++)
  {
    unsigned char readbuffer[DATASETSIZE];
    flash1.read(j * DATASETSIZE, readbuffer);
    for (int i = 0; i < DATASETSIZE; i++)
    {
      Serial.print(readbuffer[i]);
      Serial.print(',');
    }
  }
}

IRAM_ATTR int attachDataSet(uint8_t *data, uint8_t dataLength)
{

  for (int i = 0; i < dataLength; i++)
  {
    LOGGING::latestDataset[LOGGING::latestDatasetIndex++] = data[i];
    // Serial.printf("i = %d", LOGGING::latestDatasetIndex);
    //  Serial.println("data attached");
    if (LOGGING::latestDatasetIndex == 256)
    {
      // Serial.println("kakikomi kaishi");
      flash1.write((LOGGING::latestFlashPage++) << 8, LOGGING::latestDataset);
      // Serial.println("write flash");
      LOGGING::latestDatasetIndex = 0;
    }
  }
  return 0;
}

IRAM_ATTR void logTaskDelete()
{
  vTaskDelete(LOGGING::LoggingHandle);
}

IRAM_ATTR void finishLogging()
{
  LOGGING::isLoggingGoing = 0;
  // led.PWMChangeFreq(5);
  logTaskDelete();
  PIRECStopAndKill();
  // led.PWMChangeFreq(1);
}

IRAM_ATTR void loggingData(void *parameters)
{
  portTickType xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {

    // get mpu data
    int16_t mpudata[6];
    uint8_t mpudata_flashbf[17];
    unsigned long mpugettime = micros();
    mpu9250.Get(mpudata);

    // Serial.println("mpu data get");

    // set header to bf
    mpudata_flashbf[0] = MPUDATAHEAD;
    // set time to bf
    for (int i = 0; i < 4; i++)
    {
      mpudata_flashbf[1 + i] = (unsigned char)((mpugettime >> (8 * i)) % 256);
    }
    // set mpu data to bf
    for (int i = 0; i < 6; i++)
    {
      mpudata_flashbf[5 + 2 * i] = (unsigned char)(mpudata[i] % 256);
      mpudata_flashbf[6 + 2 * i] = (unsigned char)(mpudata[i] >> 8);
    }

    // set freq(lps)
    if (LOGGING::lps_counter++ == MPULPSRATIO)
    {
      // lps logging task
      unsigned char lpsdata[3];
      unsigned long lpsgettime = micros();
      lps22hb.Get(lpsdata);

      // buffer
      uint8_t lpsdata_flashbf[8];

      // set headder to buffer
      lpsdata_flashbf[0] = LPSDATAHEAD;
      // set time to buffer
      for (int i = 0; i < 4; i++)
      {
        lpsdata_flashbf[1 + i] = (unsigned char)((lpsgettime >> (8 * i)) % 256);
      }
      // set lpsdata to buffer
      for (int i = 0; i < 3; i++)
      {
        lpsdata_flashbf[5 + i] = lpsdata[i];
      }
      // Serial.println("lps data get");
      attachDataSet(lpsdata_flashbf, 8);
      // Serial.println("lps data send");
      LOGGING::lps_counter = 0;
    }

    attachDataSet(mpudata_flashbf, 17);
    // Serial.println("mpu data send");

    vTaskDelayUntil(&xLastWakeTime, LOGGINGINTERVAL / portTICK_PERIOD_MS);
  }
}

IRAM_ATTR void logTaskCreate()
{
  initVariables();
  xTaskCreate(loggingData, "Logging", 8192, NULL, 1, &LOGGING::LoggingHandle);
}

void setup()
{
  // setup buz power
  pinMode(HIGH_VOLTAGE_SW, OUTPUT);
  digitalWrite(HIGH_VOLTAGE_SW, LOW);

  // led.PWMInit(0, 1, 8, 2, 128);
  buz.PWMInit(2, 2000, 8, BUZ_SW, 0);

  PIPinsInit();

  // init serials
  Serial.begin(115200);
  Serial.println("ESP launched");
  Serial2.begin(115200, SERIAL_8N1, COMBOARDRX, COMBOARDTX);

  SPIC1.begin(VSPI, SCK1, MISO1, MOSI1);
  mpu9250.begin(&SPIC1, MPUCS, SPIFREQ);
  lps22hb.begin(&SPIC1, LPSCS, SPIFREQ);
  flash1.begin(&SPIC1, flashCS, SPIFREQ);
  initVariables();
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
      if (LOGGING::isCheckedSensor)
      {
        Serial2.print(LOGGING::sensorStatus);
        break;
      }
      LOGGING::isCheckedSensor = 1;
      int sensorStatus = 0; // 0:ok 1:not ok
      sensorStatus = checkSensor();
      // led.PWMChangeFreq(20);
      PILaunch();
      PIRECStart();
      delay(30000);
      sensorStatus = sensorStatus || ISPICAMOK();
      if (sensorStatus)
      {
        LOGGING::sensorStatus = WRONGSENSORCMD;
        Serial2.print(LOGGING::sensorStatus);
      }
      else
      {
        LOGGING::sensorStatus = CHECKSENSORCMD;
        Serial2.print(LOGGING::sensorStatus);
      }
      delay(10000);
      PIRECStopAndKill();
      // led.PWMChangeFreq(1);
      break;
    }

    case STARTLOGGINGCMD:
    {
      if (LOGGING::isLoggingGoing)
      {
        break;
      }
      LOGGING::isLoggingGoing = 1;
      Serial2.print(STARTLOGGINGCMD);
      // led.PWMChangeFreq(20);
      PILaunch();
      eraseFlash();
      PIRECStart();
      logTaskCreate();
      Serial.println("logging start");
      // led.PWMChangeFreq(10);
      break;
    }

    case STOPLOGGINGCMD:
    {
      if (!LOGGING::isLoggingGoing)
      {
        break;
      }
      Serial2.print(STOPLOGGINGCMD);
      finishLogging();
      Serial.println("logging finish by cmd");
      break;
    }

    case DATAERACECMD:
    {
      Serial.println("data erace by erace cmd");
      // led.PWMChangeFreq(20);
      eraseFlash();
      // led.PWMChangeFreq(1);
      Serial2.print(DATAERACECMD);
      break;
    }

    case ENABLEBUZCMD:
    {
      Serial2.print(ENABLEBUZCMD);
      digitalWrite(HIGH_VOLTAGE_SW, HIGH);
      buz.PWMChangeDuty(128);
      break;
    }

    case DISABLEBUZCMD:
    {
      Serial2.print(DISABLEBUZCMD);
      digitalWrite(HIGH_VOLTAGE_SW, LOW);
      buz.PWMChangeDuty(0);
      break;
    }
    }
  }

  while (Serial.available())
  {
    unsigned char cmdFromPC = Serial.read();

    switch (cmdFromPC)
    {
    case READMODECMD:
      Serial.println("flash read mode");
      readAllFlash();
      break;
    }
  }
  if (LOGGING::isLoggingGoing && (LOGGING::latestFlashPage > 65530))
  {
    finishLogging();
    Serial.println("logging finish by filling flash");
  }
}