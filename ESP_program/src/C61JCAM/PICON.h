#include <Arduino.h>

#ifndef PICON
#define PICON

// raspi const pins
#define POWPI0 4
#define TRIGPI0 5
#define STATPI0 19
#define POWPI1 18
#define TRIGPI1 23
#define STATPI1 22

void PIPinsInit();

void PILaunch();

void PIRECStart();

int ISPICAMOK(void);

void PIRECStopAndKill();

void PIPinsInit()
{
    pinMode(POWPI0, OUTPUT);
    pinMode(POWPI1, OUTPUT);
    digitalWrite(POWPI0, LOW);
    digitalWrite(POWPI1, LOW);

    pinMode(TRIGPI0, OUTPUT);
    pinMode(TRIGPI1, OUTPUT);
    digitalWrite(TRIGPI0, HIGH);
    digitalWrite(TRIGPI1, HIGH);

    pinMode(STATPI0, INPUT_PULLUP);
    pinMode(STATPI1, INPUT_PULLUP);
}

void PILaunch()
{
    digitalWrite(TRIGPI0, LOW);
    digitalWrite(TRIGPI1, LOW);
    delay(1000);
    digitalWrite(POWPI0, HIGH);
    digitalWrite(POWPI1, HIGH);
}

void PIRECStart()
{
    digitalWrite(TRIGPI0, HIGH);
    digitalWrite(TRIGPI1, HIGH);
}

int ISPICAMOK()
{
    if (digitalRead(STATPI0) == HIGH && digitalRead(STATPI1) == HIGH)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void PIRECStopAndKill()
{
    digitalWrite(TRIGPI0, LOW);
    digitalWrite(TRIGPI1, LOW);
    delay(30000);
    digitalWrite(POWPI0, LOW);
    digitalWrite(POWPI1, LOW);
    delay(1000);
    digitalWrite(TRIGPI0, HIGH);
    digitalWrite(TRIGPI1, HIGH);
}

#endif