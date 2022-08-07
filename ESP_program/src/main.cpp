#include <Arduino.h>

#include <C61JCAM/SPICONFIG.h>

#include <C61JCAM/COMCONFIG.h>
#include <C61JCAM/PICON.h>
#include <C61JCAM/PWMS.hpp>

PWM led;
PWM buz;

SPICREATE::SPICreate SPIC1;
MPU mpu9250;
Flash flash1;
LPS lps22hb;

namespace LOGGING
{
  int isAttachingData = 0;
  int latestDatasetIndex = 0;
  int latestFlashPage = 0;
  int lps_counter = 0;
  int isFlashErased = 0;   // 1:Erased 0:not Erased
  int isLoggingGoing = 0;  // 1:Going 0:not Going
  int isCheckedSensor = 0; // 1:Checked 0:not Checked
  char sensorStatus;

  QueueHandle_t flashQueue;
  uint8_t *latestDataset;
  TaskHandle_t LoggingHandle;
};

void initVariables()
{
  LOGGING::isAttachingData = 0;
  LOGGING::latestDatasetIndex = 0;
  LOGGING::latestFlashPage = 0;
  LOGGING::lps_counter = 0;
}

IRAM_ATTR int checkSensor()
{
  //(lps22hb.WhoImI() == 177) &&
  if ((mpu9250.WhoImI() == 113))
  {
    return 0;
  }
  Serial.print("LPS:");
  Serial.println(lps22hb.WhoImI());
  Serial.print("MPU:");
  Serial.println(mpu9250.WhoImI());
  return 1;
}

IRAM_ATTR void eraseFlash()
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

IRAM_ATTR int makequeue()
{
  LOGGING::flashQueue = xQueueCreate(FLASHQUEUELENGTH, sizeof(uint8_t *));
  if (LOGGING::flashQueue == NULL)
  {
    return 1;
  }
  return 0;
}

IRAM_ATTR void deletequeue()
{
  vQueueDelete(LOGGING::flashQueue);
}

IRAM_ATTR uint8_t *allocDataSet()
{
  uint8_t *dataset = new uint8_t[DATASETSIZE];
  return dataset;
}

IRAM_ATTR int attachDataSet(uint8_t *data, uint8_t dataLength)
{
  while (LOGGING::isAttachingData)
  {
    delayMicroseconds(10);
  }
  LOGGING::isAttachingData = 1;
  for (int i = 0; i < dataLength; i++)
  {
    if (LOGGING::latestDatasetIndex % 256 == 0)
    {
      LOGGING::latestDataset = allocDataSet();
      if (LOGGING::latestDataset == NULL)
      {
        return 1;
      }
    }
    LOGGING::latestDataset[LOGGING::latestDatasetIndex] = data[i];
    LOGGING::latestDatasetIndex++;
    if (LOGGING::latestDatasetIndex == 256)
    {
      xQueueSend(LOGGING::flashQueue, &LOGGING::latestDataset, 0);
      LOGGING::latestDatasetIndex = 0;
    }
  }
  LOGGING::isAttachingData = 0;
  return 0;
}

IRAM_ATTR void logTaskDelete()
{
  vTaskDelete(LOGGING::LoggingHandle);
  deletequeue();
}

IRAM_ATTR void writeFlashFromQueue()
{
  if (LOGGING::latestFlashPage > 65530)
  {
    led.PWMChangeFreq(5);
    logTaskDelete();
    PIRECStopAndKill();
    led.PWMChangeFreq(1);
  }
  uint8_t *dataset;
  if (xQueueReceive(LOGGING::flashQueue, &dataset, 0) == pdTRUE)
  {
    flash1.write(LOGGING::latestFlashPage * 256, dataset);
    LOGGING::latestFlashPage++;
    delete[] dataset;
  }
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
      attachDataSet(lpsdata_flashbf, 8);
      LOGGING::lps_counter = 0;
    }

    attachDataSet(mpudata_flashbf, 17);

    writeFlashFromQueue();

    vTaskDelayUntil(&xLastWakeTime, LOGGINGINTERVAL / portTICK_PERIOD_MS);
  }
}

IRAM_ATTR void logTaskCreate()
{
  initVariables();
  makequeue();
  xTaskCreate(loggingData, "Logging", 8192, NULL, 1, &LOGGING::LoggingHandle);
}

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
      led.PWMChangeFreq(20);
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
      led.PWMChangeFreq(1);
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
      led.PWMChangeFreq(20);
      PILaunch();
      eraseFlash();
      PIRECStart();
      logTaskCreate();
      led.PWMChangeFreq(10);
      break;
    }

    case STOPLOGGINGCMD:
    {
      if (!LOGGING::isLoggingGoing)
      {
        break;
      }
      LOGGING::isLoggingGoing = 0;
      Serial2.print(STOPLOGGINGCMD);
      led.PWMChangeFreq(5);
      logTaskDelete();
      PIRECStopAndKill();
      led.PWMChangeFreq(1);
      break;
    }

    case DATAERACECMD:
    {
      Serial2.print(DATAERACECMD);
      led.PWMChangeFreq(20);
      eraseFlash();
      led.PWMChangeFreq(1);
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
    Serial.print("recieved cmd from Serial0 is '");
    Serial.print(cmdFromPC);
    Serial.println("'");

    switch (cmdFromPC)
    {
    case READMODECMD:
      readAllFlash();
      break;
    }
  }

  delay(1000);
}