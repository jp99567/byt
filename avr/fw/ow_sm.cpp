#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "ow_sm.h"

namespace ow{
void response(enum ResponseCode code);
extern int gOwBitsCount;
extern uint8_t gOwData[12];
}

namespace {

enum OwState
{
	eOwIdle,
	eOwInitWaitResetPulseFinished,
	eOwInitWaitPresence,
	eOwInitWaitPresenceCleared,
	eOwInitWaitRecovery,
	eOwReadBits,
	eOwWriteBits,
	eOwSearch,
};

enum OwBitIoState
{
    eOwBitIoFinishedOk,
    eOwBitIoFinishedError,
	eOwBitIoWaitSampleEvent,
	eOwBitIoWaitSlotEnd,
};

enum OwStrongPowerRequest
{
    eOwStrongPower0,
    eOwStrongPowerReq,
    eOwStrongPowerKeepAfterWrite,
};

static enum OwState sState = eOwIdle;
static enum OwStrongPowerRequest sStrongPowerRequest;
static enum OwBitIoState sBitIoState = eOwBitIoFinishedOk;
static uint8_t sBitidx;

void bus_pull(void)
{
	PORTA &= ~_BV(PA0);
	DDRA |= _BV(PA0);
}

void bus_release(void)
{
	DDRA &= ~_BV(PA0);
}

void bus_power_strong(void)
{
	PORTA |= _BV(PA0);
	DDRA |= _BV(PA0);
}

bool bus_active(void)
{
	return not(PINA & _BV(PA0));
}

ow::ResponseCode gResponseCode;

constexpr uint8_t clockdiv(const unsigned us)
{
	if(us*8 < 255)
		return 0b001; //No prescaling
	if(us < 255)
		return 0b010; //clkI/O/8 (From prescaler)
	if(us / 8 < 255)
		return 0b011; // clkI/O/64 (From prescaler)
	else{
		return 0;
	}
}

constexpr uint8_t ticks(unsigned us)
{
	uint8_t v = 0;
	if(us*8 < 255)
		v = us * 8; //No prescaling
	else if(us < 255)
		v = us; //clkI/O/8 (From prescaler)
	else if(us / 8 < 255)
		v = us / 8; // clkI/O/64 (From prescaler)
	return 255 - v + 1;
}

template<uint8_t ClockDiv, uint8_t Ticks>
void setT0Timeout()
{
	static_assert(Ticks > 0, "invalid ticks value");
	TCNT0 = Ticks;
	TCCR0A = ClockDiv;
}

#define SCHEDULE_TIMEOUT(us) setT0Timeout<clockdiv((us)), ticks((us))>()

constexpr unsigned cDurFallingEdge = 10;
constexpr unsigned cDurResetPulse = 489;
constexpr unsigned cDurWaitPresence = 90;
constexpr unsigned cDurWaitPresenceCleared = cDurResetPulse-cDurWaitPresence;
constexpr unsigned cDurReadSample = 5;
constexpr unsigned cDurSlot = 60;
constexpr unsigned cDurWrite0Pulse = 60;
constexpr unsigned cDurWrite0Relax = 80 - cDurWrite0Pulse;
constexpr unsigned cDurWrite1Relax = 80;

static void set_state_idle()
{
	sState = eOwIdle;
	TCCR0A = 0; //timer off
}

static void wait_reset_pulse(void)
{
	if(not bus_active())
	{
		ow::response(ow::eOwBusFailure1);
		set_state_idle();
		bus_release();
		return;
	}
	bus_release();
	SCHEDULE_TIMEOUT(cDurWaitPresence);
	sState = eOwInitWaitPresence;
}

static void wait_presence_pulse(void)
{
	if(bus_active()){
		sState = eOwInitWaitPresenceCleared;
		SCHEDULE_TIMEOUT(cDurWaitPresenceCleared);
	}
	else{
		ow::response(ow::eOwNoPresence);
		set_state_idle();
	}
}

static void wait_presence_cleared(void)
{
	if(!bus_active())
	{
		ow::response(ow::eOwPresenceOk);
	}
	else
	{
		ow::response(ow::eOwBusFailure3);
	}
	set_state_idle();
}

static void sample(bool v)
{
	unsigned byte = sBitidx / 8;
	unsigned bitmask = 1 << sBitidx % 8;

	if(v)
	{
		ow::gOwData[byte] |= bitmask;
	}
	else
	{
		ow::gOwData[byte] &= ~bitmask;
	}
}

static bool bitval(void)
{
    auto byte = sBitidx / 8;
    auto bitmask = _BV(sBitidx % 8);
    return ow::gOwData[byte] & bitmask;
}

void bus_releasy_after_write()
{
	   if(sStrongPowerRequest == eOwStrongPowerKeepAfterWrite){
		   bus_power_strong();
		   sStrongPowerRequest = eOwStrongPower0;
	   }
	   else{
		   bus_release();
	   }
}

void start_write_bit()
{
	bus_pull();
	if(bitval()){ //~ 1us between
		bus_releasy_after_write();
		sBitIoState = eOwBitIoWaitSlotEnd;
		SCHEDULE_TIMEOUT(cDurWrite1Relax);
	}
	else{
		sBitIoState = eOwBitIoWaitSampleEvent;
		SCHEDULE_TIMEOUT(cDurWrite0Pulse);
	}
}

static enum OwBitIoState write_bit(void)
{
    switch(sBitIoState)
    {
      case eOwBitIoWaitSampleEvent: //case of write0 only
        SCHEDULE_TIMEOUT(cDurWrite0Relax);
        bus_releasy_after_write();
        sBitIoState = eOwBitIoWaitSlotEnd;
        break;
      case eOwBitIoWaitSlotEnd:
        sBitIoState = eOwBitIoFinishedOk;
        break;
      default:
        sBitIoState = eOwBitIoFinishedError;
       break;
    }

    return sBitIoState;
}

void start_read_bit()
{
	bus_pull();
	_delay_loop_1(3);
	bus_release();
	SCHEDULE_TIMEOUT(cDurReadSample);
    sBitIoState = eOwBitIoWaitSampleEvent;
}

static enum OwBitIoState read_bit(void)
{
	switch(sBitIoState)
	{
	  case eOwBitIoWaitSampleEvent:
	   {
		   auto v = ! bus_active();
		   sample(v);
		   sBitIoState = eOwBitIoWaitSlotEnd;
		   SCHEDULE_TIMEOUT(cDurSlot);
	   }
	   break;
	  case eOwBitIoWaitSlotEnd:
	   {
		   sBitIoState = eOwBitIoFinishedOk;
	   }
	   break;
	  default:
		 sBitIoState = eOwBitIoFinishedError;
	   break;
	}

	return sBitIoState;
}

static void do_write_bits(void)
{
    enum OwBitIoState status = write_bit();

    switch(status)
    {
        case eOwBitIoFinishedError:
            ow::response(ow::eOwWriteBitsFailure);
            bus_release();
            set_state_idle();
          break;
        case eOwBitIoFinishedOk:
            ++sBitidx;
            if(sBitidx >= ow::gOwBitsCount)
            {
                ow::response(ow::eOwWriteBitsOk);
                set_state_idle();
            }
            else{
            	if(sStrongPowerRequest == eOwStrongPowerReq
            		&& sBitidx == ow::gOwBitsCount-1){
            		sStrongPowerRequest = eOwStrongPowerKeepAfterWrite;
            	}
            	start_write_bit();
            }
          break;
        default:
          break;
    }
}

static void do_read_bits(void)
{
    enum OwBitIoState status = read_bit();

    switch(status)
    {
        case eOwBitIoFinishedError:
            ow::response(ow::eOwReadBitsFailure);
            bus_release();
            set_state_idle();
          break;
        case eOwBitIoFinishedOk:
            ++sBitidx;
            if(sBitidx >= ow::gOwBitsCount)
            {
                ow::response(ow::eOwReadBitsOk);
                set_state_idle();
            }
            else
            {
            	start_read_bit();
            }
          break;
        default:
          break;
    }
}

void storeDirection(bool direction)
{
	ow::gOwData[1] = direction;
}

bool getDirection()
{
	return ow::gOwData[1];
}

void setBit2(bool v)
{
	uint8_t mask = 1<<2;
	ow::gOwData[0] = v ? ( ow::gOwData[0] | mask ) : (ow::gOwData[0] & ~mask);
}

static void do_search()
{
	switch(sBitidx)
	{
		case 0:
		case 1:
		{
		    enum OwBitIoState status = read_bit();

		    switch(status)
		    {
		        case eOwBitIoFinishedError:
		            ow::response(ow::eOwReadBitsFailure);
		            set_state_idle();
		            bus_release();
		          break;
		        case eOwBitIoFinishedOk:
		        	sBitidx++;
		        	if(sBitidx == 2){
		        		unsigned v = ow::gOwData[0] & 0x3;
		        		if( v == 3 ){
		        			ow::response(ow::eOwSearchResult11);
		        			set_state_idle();
		        			return;
		        		}
		        		else if(v){
		        			setBit2(v==1);
		        		}
		        		else{
		        			setBit2(getDirection());
		        		}
		        		start_write_bit();
		        	}
		        	else{
		        		start_read_bit();
		        	}
		          break;
		        default:
		          break;
		    }
		    break;
		}
		case 2:
		{
		    enum OwBitIoState status = write_bit();

		    switch(status)
		    {
		        case eOwBitIoFinishedError:
		            ow::response(ow::eOwWriteBitsFailure);
		            sState = eOwIdle;
		          break;
		        case eOwBitIoFinishedOk:
		        	{
		        		unsigned v = ow::gOwData[0] & 0x3;
		        		switch(v){
		        		case 0:
		        			ow::response(ow::eOwSearchResult00);
		        			break;
		        		case 1:
		        			ow::response(ow::eOwSearchResult0);
		        			break;
		        		case 2:
		        			ow::response(ow::eOwSearchResult1);
		        			break;
		        		default:
		        			ow::response(ow::eOwSearchResult11);
		        			break;
		        		}
		                sState = eOwIdle;
		        	}
		          break;
		        default:
		          break;
		    }
		}
		break;
	}
}

}

namespace ow{

void response(enum ResponseCode code)
{
	gResponseCode = code;
}
ResponseCode response()
{
	return gResponseCode;
}

void read_bits(uint8_t count)
{
	if(sState != eOwIdle)
	{
		response(eRspError);
		return;
	}

	response(eBusy);
	gOwBitsCount = count;
	sBitidx = 0;
	sState = eOwReadBits;
	TIFR0 |= _BV(TOV0);
	TIMSK0 = _BV(TOIE0);
	cli();
	start_read_bit();
	sei();
}

void write_bits(uint8_t count, bool strong_power_req)
{
	if(sState != eOwIdle)
	{
		response(eRspError);
		return;
	}

	response(eBusy);
	gOwBitsCount = count;
	sBitidx = 0;
    sStrongPowerRequest = strong_power_req ? eOwStrongPowerReq : eOwStrongPower0;
    sState = eOwWriteBits;
	TIFR0 |= _BV(TOV0);
	TIMSK0 = _BV(TOIE0);
	cli();
	start_write_bit();
	sei();
}

void search(bool direction)
{
	if(sState != eOwIdle)
	{
		response(eRspError);
		return;
	}

	response(eBusy);
	storeDirection(direction);
	sBitidx = 0;
	sState = eOwSearch;
	TIFR0 |= _BV(TOV0);
	TIMSK0 = _BV(TOIE0);
	cli();
	start_read_bit();
	sei();
}

void init(void)
{
	if(sState != eOwIdle)
	{
		response(eRspError);
		return;
	}

	if(bus_active())
	{
		response(eOwBusFailure0);
		bus_release();
		return;
	}

	response(eBusy);
	cli();
	SCHEDULE_TIMEOUT(cDurResetPulse);
	TIFR0 |= _BV(TOV0);
	TIMSK0 = _BV(TOIE0);
	bus_pull();
	sState = eOwInitWaitResetPulseFinished;
	sei();
}

void ow_sm_do(void)
{
	switch(sState)
	{
	case eOwIdle:
		break;
	case eOwInitWaitResetPulseFinished:
		wait_reset_pulse();
		break;
	case eOwInitWaitPresence:
		wait_presence_pulse();
		break;
	case eOwInitWaitPresenceCleared:
		wait_presence_cleared();
		break;
	case eOwReadBits:
		do_read_bits();
		break;
	case eOwWriteBits:
	    do_write_bits();
	    break;
	case eOwSearch:
		do_search();
		break;
	default:
		break;
	}
}

}

ISR(TIMER0_OVF_vect){
	ow::ow_sm_do();
}
