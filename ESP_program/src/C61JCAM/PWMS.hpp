#include <Arduino.h>

#ifndef PWMS
#define PWMS

class PWM
{
private:
    uint8_t ch;

public:
    void PWMInit(int _ch, double freq, uint8_t res_bit, uint8_t pin, uint8_t duty)
    {
        ch = _ch;
        ledcSetup(ch, freq, res_bit);
        ledcAttachPin(pin, ch);
        ledcWrite(ch, duty);
        ledcWriteTone(ch, freq);
        ledcWrite(ch, duty);
    }

    void PWMChangeFreq(double freq)
    {
        ledcWriteTone(ch, freq);
    }

    void PWMChangeDuty(uint8_t duty)
    {
        ledcWrite(ch, duty);
    }
};

#endif