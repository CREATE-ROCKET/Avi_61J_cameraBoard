#include <Arduino.h>
#include <MPU9250.h>
#include <SPICREATE.h>
#include <SPIflash.h>
#include <LPS22HB.h>

#include <C61JCAM/COMCONFIG.h>
#include <C61JCAM/PICON.h>
#include <C61JCAM/PWMS.hpp>

SPICREATE::SPICreate SPIC1;
MPU mpu9250;
Flash flash1;
LPS lps22hb;

PWM led;
PWM buz;

xTaskHandle mpuHandle;

// logging data global variable
bool flash_thread_safer = 0;
unsigned char _flash_buffer[256];
int _flash_buffer_pointer = 0;
uint32_t _page = 0;
bool isLoggingEnable = 0;
bool isDataErased = 0;
bool isCheckedSensor = 0;

IRAM_ATTR void addDataFlash(unsigned char *data, int size)
{

  // if there is another writing task
  while (flash_thread_safer)
  {
    // wait writing task
    delayMicroseconds(1);
  }
  flash_thread_safer = 1;

  for (int i = 0; i < size; i++)
  {
    _flash_buffer[_flash_buffer_pointer] = data[i];
    _flash_buffer_pointer++;
    // if _flash_buffer filled
    if (_flash_buffer_pointer == 256)
    {
      flash1.write(_page * 256, _flash_buffer);
      _page++;
      _flash_buffer_pointer = 0;
    }
  }
  flash_thread_safer = 0;
}

int _lps_counter = 0;
IRAM_ATTR void getMpuData(void *parameters)
{
  portTickType xLastWakeTime = xTaskGetTickCount();
  for (;;)
  {
    // get mpu data
    int16_t mpudata[6];
    unsigned char mpudata_flashbf[17];
    unsigned long mpugettime = micros();
    mpu9250.Get(mpudata);

    // set header to bf
    mpudata_flashbf[0] = 0xE0;
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
    if (_lps_counter++ == 50)
    {
      // lps logging task
      unsigned char lpsdata[3];
      unsigned long lpsgettime = micros();
      lps22hb.Get(lpsdata);

      // buffer
      unsigned char lpsdata_flashbf[8];

      // set headder to buffer
      lpsdata_flashbf[0] = 0xE3;
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
      // write flash(lps)
      addDataFlash(lpsdata_flashbf, 8);
      _lps_counter = 1;
    }

    // write flash(mpu)
    addDataFlash(mpudata_flashbf, 17);

    vTaskDelayUntil(&xLastWakeTime, MPU_FREQ / portTICK_PERIOD_MS);
  }
}

void initLoggingGlobalVars(void)
{
  flash_thread_safer = 0;
  _flash_buffer_pointer = 0;
  _page = 0;
  isLoggingEnable = 0;
  isDataErased = 0;
  isCheckedSensor = 0;
}

void stopLogging(void)
{
  // kill logging task
  vTaskDelete(mpuHandle);

  // shutdown raspi

  PIRECStopAndKill();
  Serial.println("stop logging complete");
}

void checkSensor()
{
  if ((lps22hb.WhoImI() == 177) && (mpu9250.WhoImI() == 113))
  {
    // camera check
    PILaunch();
    PIRECStart();
    delay(60000);
    if (ISPICAMOK())
    {
      Serial2.print(CHECKSENSORCMD);
    }
    else
    {
      Serial2.print(WRONGSENSORCMD);
    }
    delay(20000);
    PIRECStopAndKill();
  }
  else
  {
    Serial2.print(WRONGSENSORCMD); // sensor not ok
    Serial.println(lps22hb.WhoImI());
    Serial.println(mpu9250.WhoImI());
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

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

  // init spis
  SPIC1.begin(VSPI, SCK1, MISO1, MOSI1);
  mpu9250.begin(&SPIC1, MPUCS, 12000000);
  lps22hb.begin(&SPIC1, LPSCS, 12000000);
  flash1.begin(&SPIC1, flashCS, 12000000);
}

void loop()
{
  // cmd from com board
  while (Serial2.available())
  {
    char command = Serial2.read();
    // if cmd 'i' recieved, check sensor
    if (command == CHECKSENSORCMD)
    {
      if (isCheckedSensor == 0)
      {
        isCheckedSensor = 1;
        checkSensor();
      }
    }
    // if recieved other cmd, transmit same cmd
    else
    {
      isCheckedSensor = 0;
      Serial2.print(command);
    }

    // if cmd 'l' recieved, start logging
    if (command == STARTLOGGINGCMD)
    {
      if (isLoggingEnable == 0)
      {
        isLoggingEnable = 1;
        Serial.println("start logging");
        Serial.println("FLIGHTMODE");
        led.PWMChangeFreq(5);
        // init raspi
        PILaunch();
        PIRECStart();

        // mode: Flight mode
        flash1.erase();
        delay(1);

        // start: mpu and lps logging
        xTaskCreate(getMpuData, "getMPUData", 65536, NULL, 1, &mpuHandle);
        delayMicroseconds(50);
      }
    }
    // if cmd 's' recieved, stop logging and reboot ESP
    if (command == STOPLOGGINGCMD)
    {
      if (isLoggingEnable == 1)
      {
        Serial.println("stop logging");
        led.PWMChangeFreq(1);
        stopLogging();
        initLoggingGlobalVars();
        isLoggingEnable = 0;
      }
    }
    // if cmd 'd' recieved, erase flash
    if (command == DATAERACECMD)
    {
      if (isDataErased == 0)
      {
        Serial.println("erase data");
        flash1.erase();
        initLoggingGlobalVars();
        Serial2.print(COMPLETEDATAERACECMD);
      }
      isDataErased = 1;
    }
    // if cmd 'b' recieved, turn on buz *note* change high voltage sw on before launch
    if (command == ENABLEBUZCMD)
    {
      digitalWrite(HIGH_VOLTAGE_SW, HIGH);
      Serial.println("buzzer on");
      buz.PWMChangeDuty(127);
    }
    // if cmd 'u' recieved, rutn off buz
    if (command == DISABLEBUZCMD)
    {
      digitalWrite(HIGH_VOLTAGE_SW, LOW);
      Serial.println("buzzer off");
      buz.PWMChangeDuty(0);
    }
  }

  // check flash page
  while (_page > 65530)
  {
    Serial.println("flash filled");
    stopLogging();
    initLoggingGlobalVars();
  }

  // cmd from pc (change READMODE:'r'(114))
  while (Serial.available())
  {
    unsigned char command = Serial.read();
    Serial.print("recieved cmd from Serial0 is '");
    Serial.print(command);
    Serial.println("'");

    if (command == READMODECMD)
    {
      Serial.println("READMODE");
      // mode: read mode
      for (int j = 0; j < 65536; j++)
      {
        unsigned char readbuffer[256];
        for (int i = 0; i < 256; i++)
        {
          readbuffer[i] = 128;
        }
        flash1.read(j * 256, readbuffer);
        for (int i = 0; i < 256; i++)
        {
          Serial.print(readbuffer[i]);
          Serial.print(',');
        }
      }
    }
  }
  // wtd call for pc
  Serial.println("a");
  delay(1000);
}
/////////////////////////////////////////////////////////////////////////////////////////////////