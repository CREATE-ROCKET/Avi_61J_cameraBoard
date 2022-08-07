#ifndef SPICONFIG_H
#define SPICONFIG_H

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

#endif