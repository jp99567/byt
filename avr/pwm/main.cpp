#include <avr/io.h>
#include <avr/interrupt.h>

constexpr uint8_t portb_mask_spi = (1<<2)|(1<<3)|(1<<4)|(1<<5);
uint8_t portb_255_mask;
uint8_t portc_255_mask;
uint8_t portd_255_mask;

ISR(TIMER0_OVF_vect){
	PORTD &= portd_255_mask;
	PORTC &= portc_255_mask;
	PORTB &= portb_mask_spi|portb_255_mask;
}

constexpr uint8_t chN = 16;

uint8_t pwm_per[chN];

template<uint8_t id>
volatile uint8_t& selectPort(){return PORTD;}
template<>volatile uint8_t& selectPort<5>() { return PORTB; }
template<>volatile uint8_t& selectPort<6>() { return PORTB; }
template<>volatile uint8_t& selectPort<10>() { return PORTB; }

template<>volatile uint8_t& selectPort<11>() { return PORTC; }
template<>volatile uint8_t& selectPort<12>() { return PORTC; }
template<>volatile uint8_t& selectPort<13>() { return PORTC; }
template<>volatile uint8_t& selectPort<14>() { return PORTC; }
template<>volatile uint8_t& selectPort<15>() { return PORTC; }

template<uint8_t pin>
uint8_t selectPin(){return 0;}
template<>uint8_t selectPin<0>() { return PD0; }
template<>uint8_t selectPin<1>() { return PD1; }
template<>uint8_t selectPin<2>() { return PD2; }
template<>uint8_t selectPin<3>() { return PD3; }
template<>uint8_t selectPin<4>() { return PD4; }

template<>uint8_t selectPin<5>() { return PB6; }
template<>uint8_t selectPin<6>() { return PB7; }
template<>uint8_t selectPin<7>() { return PD5; }
template<>uint8_t selectPin<8>() { return PD6; }
template<>uint8_t selectPin<9>() { return PD7; }
template<>uint8_t selectPin<10>() { return PB0; }

template<>uint8_t selectPin<11>() { return PC5; }
template<>uint8_t selectPin<12>() { return PC4; }
template<>uint8_t selectPin<13>() { return PC3; }
template<>uint8_t selectPin<14>() { return PC2; }
template<>uint8_t selectPin<15>() { return PC1; }

template<uint8_t id>
void setch(){ selectPort<id>() |= _BV(selectPin<id>());}

template<uint8_t i>
void pwm()
{
	if(i<chN){
		if(pwm_per[i] > TCNT0){
			setch<i>();
		}
		pwm<i+1>();
	}
}

template<>
void pwm<chN>()
{
}


int main() 
{
	for(int i=0; i<chN; i++){
		pwm_per[i] = 10+i;
	}

  TCCR0 = 0b011;
  TIMSK = (1<<0);
  sei();

  while(1){
	  pwm<0>();
  }

  return 0;
}


