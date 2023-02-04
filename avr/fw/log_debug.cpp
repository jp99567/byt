#include "log_debug.h"


#ifdef LOG_DEBUG

#include <avr/io.h>

namespace {

int USART0_Transmit(char c, FILE*);
static FILE f_usart0;

void USART0_Init()
{
	UBRR0 = 0;
	UCSR0A = _BV(U2X0);
	UCSR0B = _BV(TXEN0);
	f_usart0.flags = _FDEV_SETUP_WRITE;
	f_usart0.put = &USART0_Transmit;
	stdout = &f_usart0;
}

int USART0_Transmit(char c, FILE*)
{
	if( c == '\0')
		c = '\n';
	while ( ! ( UCSR0A & (1<<UDRE0)));
	UDR0 = c;
	return 0;
}
}

void init_log_debug()
{
	USART0_Init();
}

#else
void init_log_debug(){}
#endif
