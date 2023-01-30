#include "gpio.h"

namespace port {
constexpr uint8_t id(uint8_t pin)
{
	return pin >> 3;
}

constexpr uint8_t mask(uint8_t pin)
{
	return 1 << (pin & 0b111);
}
}

bool getDigInVal(uint8_t pin)
{
	uint8_t portid = port::id(pin);
	uint8_t mask = port::mask(pin);

		switch(portid){
		case 0:	return PINA & mask;
		case 1: return PINB & mask;
		case 2: return PINC & mask;
		case 3: return PIND & mask;
		case 4: return PINE & mask;
		case 5:	return PINF & mask;
		case 6:	return PING & mask;
		default:
			return false;
		}
}

volatile uint8_t& getPortRef(uint8_t portid)
{
		switch(portid){
		case 0:	return PORTA;
		case 1: return PORTB;
		case 2: return PORTC;
		case 3: return PORTD;
		case 4: return PORTE;
		case 5:	return PORTF;
		case 6:	return PORTG;
		default:
			return PORTG;
		}
}

volatile uint8_t& getDirRef(uint8_t portid)
{
		switch(portid){
		case 0:	return DDRA;
		case 1: return DDRB;
		case 2: return DDRC;
		case 3: return DDRD;
		case 4: return DDRE;
		case 5:	return DDRF;
		case 6:	return DDRG;
		default:
			return DDRG;
		}
}

void setDirOut(uint8_t pin)
{
	uint8_t portid = port::id(pin);
	uint8_t mask = port::mask(pin);
	auto& ddr = getDirRef(portid);
	ddr |= mask;
}

void setDigOut(uint8_t pin, bool v)
{
	uint8_t portid = port::id(pin);
	uint8_t mask = port::mask(pin);
	auto& port = getPortRef(portid);

	if(v){
		port |= mask;
	}
	else{
		port &= ~mask;
	}
}

namespace test {
void setDDR(uint8_t portid, uint8_t val)
{
	switch(portid){
	case 0:
		DDRA = val;
		break;
	case 1:
		DDRB = val;
		break;
	case 2:
		DDRC = val;
		break;
	case 3:
		DDRD = val & ~_BV(PD6);
		break;
	case 4:
		DDRE = val;
		break;
	case 5:
		DDRF = val;
		break;
	case 6:
		DDRG = val & 0x1F;
		break;
	default:
		break;
	}
}

void setPORT(uint8_t portid, uint8_t val)
{
	switch(portid){
	case 0:
		PORTA = val;
		break;
	case 1:
		PORTB = val;
		break;
	case 2:
		PORTC = val;
		break;
	case 3:
		PORTD = val;
		break;
	case 4:
		PORTE = val;
		break;
	case 5:
		PORTF = val;
		break;
	case 6:
		PORTG = val & 0x1F;
		break;
	default:
		break;
	}
}

void setPIN(uint8_t portid, uint8_t val)
{
	switch(portid){
	case 0:
		PINA = val;
		break;
	case 1:
		PINB = val;
		break;
	case 2:
		PINC = val;
		break;
	case 3:
		PIND = val;
		break;
	case 4:
		PINE = val;
		break;
	case 5:
		PINF = val;
		break;
	case 6:
		PING = val & 0x1F;
		break;
	default:
		break;
	}
}

uint8_t getDDR(uint8_t portid)
{
		switch(portid){
		case 0:	return DDRA;
		case 1: return DDRB;
		case 2: return DDRC;
		case 3: return DDRD;
		case 4: return DDRE;
		case 5:	return DDRF;
		case 6:	return DDRG;
		default:
			return DDRG;
		}
}

uint8_t getPORT(uint8_t portid)
{
		switch(portid){
		case 0:	return PORTA;
		case 1: return PORTB;
		case 2: return PORTC;
		case 3: return PORTD;
		case 4: return PORTE;
		case 5:	return PORTF;
		case 6:	return PORTG;
		default:
			return PORTG;
		}
}

uint8_t getPIN(uint8_t portid)
{
		switch(portid){
		case 0:	return PINA;
		case 1: return PINB;
		case 2: return PINC;
		case 3: return PIND;
		case 4: return PINE;
		case 5:	return PINF;
		case 6:	return PING;
		default:
			return PING;
		}
}

}
