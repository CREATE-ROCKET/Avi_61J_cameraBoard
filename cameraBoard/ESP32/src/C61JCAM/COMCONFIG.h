#ifndef COMCONFIG
#define COMCONFIG

// recieve cmd const
#define STARTLOGGINGCMD 'l'
#define STOPLOGGINGCMD 's'
#define DATAERACECMD 'd'
#define ENABLEBUZCMD 'b'
#define DISABLEBUZCMD 'u'
#define CHECKSENSORCMD 'i'
// transmit cmd const
#define COMPLETEDATAERACECMD 'f'
#define WRONGSENSORCMD 'o'
// teraterm cmd const
#define READMODECMD 'r'
#define PIDATATRANSFER 'a'

//あとでけす
// spi const
#define SCK1 12
#define MOSI1 14
#define MISO1 27
#define MPUCS 26
#define flashCS 33
#define LPSCS 25
#define MPU_FREQ 1
#define LPS_FREQ 20

// serial const
#define COMBOARDRX 17
#define COMBOARDTX 16

// buzzer const
#define HIGH_VOLTAGE_SW 32
#define BUZ_SW 21

#endif