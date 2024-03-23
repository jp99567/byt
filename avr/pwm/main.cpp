#ifdef __linux__
#include<cinttypes>
#include<iostream>
#include<array>
uint8_t TCNT0;
#else
#include <avr/io.h>
#include <avr/interrupt.h>
#endif

constexpr uint8_t chN = 16;
uint8_t pwm_per[chN];
constexpr uint8_t portb_mask_spi = (1<<2)|(1<<3)|(1<<4)|(1<<5);
uint8_t portb_255_mask;
uint8_t portc_255_mask;
uint8_t portd_255_mask;
uint8_t spi_bytes_in = 0;

#ifdef __linux__
std::array<std::array<char,256>,chN> viz;
std::array<bool,chN> gClearChMask;
std::array<bool,chN> gValCh;
void ISR_TIMER0_OVF()
{
    for(int i=0; i<chN; ++i){
        if(not gClearChMask[i])
            gValCh[i] = false;
    }
}
#else
ISR(TIMER0_OVF_vect)
{
	PORTD &= portd_255_mask;
	PORTC &= portc_255_mask;
	PORTB &= portb_mask_spi|portb_255_mask;
	if(PORTB & _BV(PB2))
		spi_bytes_in = 0;
}
#endif

#ifdef __linux__
template<uint8_t id>
void setch()
{
    //std::cout << "set" << id << std::endl;
    gValCh[id] = true;
}
template<uint8_t id>
void setClerMaskCh(bool setMask)
{
    //std::cout << "set" << id << "mask:" << setMask << std::endl;
    gClearChMask[id] = setMask;
}
#else
template<uint8_t id>
volatile uint8_t& selectPort(){return PORTD;}
template<uint8_t id>
volatile uint8_t& selectPort255Mask(){return portd_255_mask;}
template<>volatile uint8_t& selectPort<5>() { return PORTB; }
template<>volatile uint8_t& selectPort255Mask<5>() { return portb_255_mask; }
template<>volatile uint8_t& selectPort<6>() { return PORTB; }
template<>volatile uint8_t& selectPort255Mask<6>() { return portb_255_mask; }
template<>volatile uint8_t& selectPort<10>() { return PORTB; }
template<>volatile uint8_t& selectPort255Mask<10>() { return portb_255_mask; }

template<>volatile uint8_t& selectPort<11>() { return PORTC; }
template<>volatile uint8_t& selectPort255Mask<11>() { return portc_255_mask; }
template<>volatile uint8_t& selectPort<12>() { return PORTC; }
template<>volatile uint8_t& selectPort255Mask<12>() { return portc_255_mask; }
template<>volatile uint8_t& selectPort<13>() { return PORTC; }
template<>volatile uint8_t& selectPort255Mask<13>() { return portc_255_mask; }
template<>volatile uint8_t& selectPort<14>() { return PORTC; }
template<>volatile uint8_t& selectPort255Mask<14>() { return portc_255_mask; }
template<>volatile uint8_t& selectPort<15>() { return PORTC; }
template<>volatile uint8_t& selectPort255Mask<15>() { return portc_255_mask; }

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

template<uint8_t id>
void setClerMaskCh(bool setMask)
{
    if(setMask)
        selectPort255Mask<id>() |= _BV(selectPin<id>());
    else
        selectPort255Mask<id>() &= ~_BV(selectPin<id>());
}
#endif

template<uint8_t i>
void pwm()
{
	if(i<chN){
        if(pwm_per[i] > 0 && pwm_per[i] >= TCNT0){
			setch<i>();
		}
		pwm<i+1>();
	}
}

template<>
void pwm<chN>()
{
}

template<uint8_t i>
void setClearMask_h(int8_t id, bool setMask)
{
    if(i<chN){
        if(i == id){
            setClerMaskCh<i>(setMask);
            if(setMask)
                setch<i>();
            return;
        }
        setClearMask_h<i+1>(id, setMask);
    }
}

template<>
void setClearMask_h<chN>(int8_t id, bool setMask)
{
}

void setClearMask(int8_t id, bool setMask)
{
    setClearMask_h<0>(id, setMask);
}

void init()
{
#ifdef __linux__
#else
    for(int i=0; i<chN; i++){
        pwm_per[i] = 10+i;
    }

    TCCR0 = 0b011;
    TIMSK = (1<<0);
    SPCR = _BV(SPE)|_BV(SPIE);
    sei();
#endif
}

#ifdef __linux__
void ISR_STC()
{
    if (spi_bytes_in < chN) {
        std::cout << "set ch" << (int)spi_bytes_in << ":\n";
        int val;
        std::cin >> val;
        pwm_per[spi_bytes_in] = (uint8_t)val;
        setClearMask(spi_bytes_in++, val==255);
    }
}
#else
ISR(SPI_STC_vect)
{
    pwm_per[spi_bytes_in] = SPDR;
}
#endif

int main() 
{
    init();
#ifdef __linux__
    //while(true){
        while(spi_bytes_in < chN){
            ISR_STC();
        }
        spi_bytes_in = 0;

        ISR_TIMER0_OVF();

        int i = 255;
        while(i >= 0 ){
            TCNT0 = i--;
            pwm<0>();
            for(int ch=0; ch<chN; ++ch)
                viz[ch][TCNT0] = gValCh[ch] ? 'X' : '_';
        }

        for(int row=0; row<chN; ++row){
            for(int col=0; col<256; ++col){
                std::cout << viz[row][col];
            }
            std::cout << "|\n";
        }
    //}
#else
    while(true){
        pwm<0>();
    }
#endif

  return 0;
}


