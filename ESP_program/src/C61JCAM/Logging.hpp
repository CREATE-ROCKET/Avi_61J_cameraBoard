#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>
#include <SPICREATE.h>
#include <MPU9250.h>
#include <SPIflash.h>
#include <LPS22HB.h>

#define SCK1 12
#define MOSI1 14
#define MISO1 27
#define MPUCS 26
#define flashCS 33
#define LPSCS 25
#define SPIFREQ 12000000

#define LOGGINGINTERVAL 1
#define MPULPSRATIO 20

#define MPUDATAHEAD 0xE0
#define LPSDATAHEAD 0xE3

#define DATASETSIZE 256
#define FLASHQUEUELENGTH 16

class LOGGING
{
private:
    static SPICREATE::SPICreate SPIC1;
    static MPU mpu9250;
    static Flash flash1;
    static LPS lps22hb;
    static int isFlashErased;  // 1:Erased 0:not Erased
    static int isLoggingGoing; // 1:Going 0:not Going
    static QueueHandle_t flashQueue;

    static void initVariables();

    // return value 0:ok 1:Failed alloc dataset
    static uint8_t *allocDataSet();
    static void freeDataSet(uint8_t *);
    // return ...?

    static int isAttachingData; // 1:busy 0:not busy
    static uint8_t *latestDataset;
    static int latestDatasetIndex;

    // return value 0:ok 1:failed make queue
    static int makequeue();
    static void deletequeue();

    static void writeFlashFromQueue();
    static int latestFlashPage;

    static int lps_counter;
    static void loggingData(void *);

    static TaskHandle_t LoggingHandle;

public:
    static void initSPI();

    // return value 0:ok 1:error
    static int checkSensor();

    static void eraseFlash();
    static void readAllFlash();

    // return value 0:ok 1:failed attach data
    static int attachDataSet(uint8_t *, uint8_t);

    static void logTaskCreate();
    static void logTaskDelete();
};
#endif