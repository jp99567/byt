#include "pwm16_at90.h"
#include <avr/io.h>

namespace Pwm16At90 {

void init(uint8_t prescaler, uint16_t top)
{
    ICR3 = top;
    TCCR3B = 1 << WGM33; //mode 8 PWM, Phase and Frequency Correct
    TCCR3B |= (prescaler & 0x07);
}

void enableOut(Channel ch)
{
	switch(ch)
	{
	case Channel::PE3_OC3A:
		TCCR3A |= (1 << COM3A1);
		DDRE |= _BV(PE3);
		break;
	case Channel::PE4_OC3B:
		TCCR3A |= (1 << COM3B1);
		DDRE |= _BV(PE4);
		break;
	case Channel::PE5_OC3C:
		TCCR3A |= (1 << COM3C1);
		DDRE |= _BV(PE5);
		break;
	default:
		break;
	}
}

void setPwm(Channel ch, uint16_t val)
{
	switch(ch)
	{
	case Channel::PE3_OC3A:
		OCR3A = val;
		break;
	case Channel::PE4_OC3B:
		OCR3B = val;
		break;
	case Channel::PE5_OC3C:
		OCR3C = val;
		break;
	default:
		break;
	}
}

}
