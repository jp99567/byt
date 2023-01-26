#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "ow.h"

ISR(TIMER0_OVF_vect){
	PIND = _BV(PD0);
}

constexpr uint8_t MCUSR_EXTENSION_NOT_BOOTLOADER = _BV(7);

namespace ow {
unsigned timer(void)
{
	return 0;
}

void send_status(enum ResponseCode code)
{

}

void send_status_with_data(enum ResponseCode code)
{

}

void send_status_with_param(enum ResponseCode code)
{

}

int gOwBitsCount;
uint8_t gOwData[12];
}

constexpr uint8_t u8min(uint8_t a, uint8_t b){
	return a < b ? a : b;
}

static uint8_t msg[8] __attribute__ ((section (".noinit")));
static uint8_t msglen __attribute__ ((section (".noinit")));
bool can_rx()
{
	for(uint8_t id=13; id<=14; ++id){
		CANPAGE = ( id << MOBNB0 );
		if(CANSTMOB & _BV(RXOK)){
			msglen = u8min(CANCDMOB & 0x0F, 8);

			for(uint8_t i=0; i<msglen; ++i)
				msg[i] = CANMSG;
			CANSTMOB = 0;
			CANCDMOB = ( 1 << CONMOB1) | ( 1 << IDE );
			if(msglen > 0)
				return true;
		}
	}
	return false;
}

void can_tx(void) {
	CANPAGE = ( 12 << MOBNB0 );
	while ( CANEN1 & ( 1 << ENMOB12 ) ); // Wait for MOb 0 to be free
	CANSTMOB = 0x00;    	// Clear mob status register
	for(uint8_t i=0; i<msglen; ++i)
		CANMSG = msg[i];

	CANCDMOB = ( 1 << CONMOB0 ) | ( 1 << IDE ) | ( msglen << DLC0 ); 	// Enable transmission, data length=1 (CAN Standard rev 2.0B(29 bit identifiers))
	while ( ! ( CANSTMOB & ( 1 << TXOK ) ) );	// wait for TXOK flag set
	CANCDMOB = 0x00;
	CANSTMOB = 0x00;
}

void init()
{
	CANTCON = 99;
	TCCR0A = 0b101;
	DDRD |= _BV(PD0);
	PORTD |= _BV(PD0);
	TIMSK0 = 1;
	sei();
}

int main() {

	init();

	msglen = 1;
	msg[0] = 0xCD;
	can_tx();
	while(true){
		can_rx();
		switch(msg[0])
		{
		case 's':
			msglen = 8;
			msg[1] = CANTEC;
			msg[2] = CANREC;
			msg[3] = MCUSR|MCUSR_EXTENSION_NOT_BOOTLOADER;
			msg[4] = CANIDT1;
			msg[5] = CANIDT2;
			msg[6] = CANIDT3;
			msg[7] = CANIDT4;
			break;
		default:
			break;
		}

		if(CANGIT & _BV(OVRTIM)){
			CANGIT |= _BV(OVRTIM);
			can_tx();
		}
	}
	return 0;
}

void setPort(uint8_t portid, uint8_t val)
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
		PORTD = val & ~_BV(PD6);
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

void setDir(uint8_t portid, uint8_t val)
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
