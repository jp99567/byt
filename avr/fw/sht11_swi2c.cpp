#include <stdint.h>
#include <avr/io.h>
#include "sht11_swi2c.h"

namespace sht11 {

void init_hw()
{
	DDRC |= _BV(PC5);
}

void clk0()
{
    PORTC &= ~_BV(PC5);
}

void clk1()
{
	PORTC |= _BV(PC5);
}

void data_pull()
{
	DDRC |= _BV(PC3);
}

void data_release()
{
	DDRC &= ~_BV(PC3);
}

bool data()
{
    return PINC & _BV(PC3);
}

}
