
#include "rpm_iface.h"
#include "common.h"

extern void bus_pull(void);
extern void bus_release(void);
extern void bus_power_strong(void);
extern int bus_active(void);
extern unsigned timer(void);
extern void send_status(enum ResponseCode code);
extern void send_status_with_data(enum ResponseCode code);
extern void send_status_with_param(enum ResponseCode code);
extern int gOwBitsCount;
extern union OwData gOwData;

void ow_init(void);
void ow_search(int direction);
void ow_read_bits(void);
void ow_write_bits(int strong_power_req);
void ow_sm_do(void);

enum OwState
{
	eOwIdle,
	eOwInitWaitResetPulseFallingEdge,
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
	eOwBitReadPulse,
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
static unsigned sT0;
static enum OwStrongPowerRequest sStrongPowerRequest;
static enum OwBitIoState sBitIoState = eOwBitIoFinishedOk;
static int sBitidx;

#define US2TICK(us) ((us)*200)

#define cDurFallingEdge US2TICK(10)
#define cDurResetPulse US2TICK(480)
#define cDurWaitPresenceT (cDurResetPulse + US2TICK(60))
#define cDurPresenceMinT US2TICK(15)
#define cDurPresenceMaxT US2TICK(240)
#define cDurInitMasterRx US2TICK(480)
#define cDurReadInitT US2TICK(6)
#define cDurReadSample US2TICK(15)
#define cDurWrite0Pulse US2TICK(64)
#define cDurWrite1Pulse US2TICK(6)
#define cDurSlot US2TICK(70)

static int timeout(unsigned delay)
{
	return (timer() - sT0) > delay;
}

static void wait_reset_pulse_check1(void)
{
    if(bus_active())
    {
        sState = eOwInitWaitResetPulseFinished;
        unsigned t = timer();
        gOwData.param = t - sT0;
        sT0 = t;
    }
    else if(timeout(cDurFallingEdge))
	{
		bus_release();
		send_status(eOwBusFailure1);
		sState = eOwIdle;
	}
}

static void wait_reset_pulse(void)
{
	if(timeout(cDurResetPulse))
	{
		bus_release();
		sState = eOwInitWaitPresence;
	}
}

static void wait_presence_pulse(void)
{
	if(timeout(cDurWaitPresenceT))
	{
		sState = eOwInitWaitPresenceCleared;
		sT0 = timer();
	}
}

static void wait_presence_cleared(void)
{
	if(!bus_active())
	{
		if(timeout(cDurPresenceMinT))
		{
		    sState = eOwInitWaitRecovery;
		}
		else
		{
			send_status_with_param(eOwNoPresence);
			sState = eOwIdle;
		}
	}
	else if(timeout(cDurPresenceMaxT))
	{
		send_status_with_param(eOwBusFailureTimeout);
		sState = eOwIdle;
	}
}

static void wait_init_recovery(void)
{
    if(timeout(cDurInitMasterRx))
    {
        send_status_with_param(eOwPresenceOk);
        sState = eOwIdle;
    }
}

static void sample(void)
{
	unsigned byte = sBitidx / 8;
	unsigned bitmask = 1 << sBitidx % 8;

	if( ! bus_active() )
	{
		gOwData.b[byte] |= bitmask;
	}
	else
	{
		gOwData.b[byte] &= ~bitmask;
	}
}

static int bitval(void)
{
    unsigned byte = sBitidx / 8;
    unsigned bitmask = 1 << sBitidx % 8;
    return gOwData.b[byte] & bitmask;
}

static enum OwBitIoState write_bit(void)
{
    switch(sBitIoState)
    {
      case eOwBitIoFinishedOk:
      case eOwBitIoFinishedError:
       {
         if(bus_active())
         {
            sBitIoState = eOwBitIoFinishedError;
         }
         else
         {
            bus_pull();
            sT0 = timer();
            sBitIoState = eOwBitIoWaitSampleEvent;
         }
       }
       break;
      case eOwBitIoWaitSampleEvent:
       {
          if( timeout( bitval()
             ? cDurWrite1Pulse
             : cDurWrite0Pulse ) )
          {
              if(sStrongPowerRequest == eOwStrongPowerKeepAfterWrite)
              {
                  bus_power_strong();
                  sStrongPowerRequest = eOwStrongPower0;
              }
              else
              {
                  bus_release();
              }
              sBitIoState = eOwBitIoWaitSlotEnd;
          }
       }
       break;
      case eOwBitIoWaitSlotEnd:
       {
           if(timeout(cDurSlot)){
               sBitIoState = eOwBitIoFinishedOk;
           }
       }
       break;
      default:
         sBitIoState = eOwBitIoFinishedError;
       break;
    }

    return sBitIoState;
}

static enum OwBitIoState read_bit(void)
{
	switch(sBitIoState)
	{
	  case eOwBitIoFinishedOk:
	  case eOwBitIoFinishedError:
	   {
		 if(bus_active())
		 {
			sBitIoState = eOwBitIoFinishedError;
		 }
		 else
		 {
			bus_pull();
			sT0 = timer();
			sBitIoState = eOwBitReadPulse;
		 }
	   }
	   break;
      case eOwBitReadPulse:
       {
          if(timeout(cDurReadInitT))
          {
              bus_release();
              sBitIoState = eOwBitIoWaitSampleEvent;
          }
       }
       break;
	  case eOwBitIoWaitSampleEvent:
	   {
		   if(timeout(cDurReadSample))
		   {
			   sample();
			   sBitIoState = eOwBitIoWaitSlotEnd;
		   }
	   }
	   break;
	  case eOwBitIoWaitSlotEnd:
	   {
		   if(timeout(cDurSlot))
			   sBitIoState = eOwBitIoFinishedOk;
	   }
	   break;
	  default:
		 sBitIoState = eOwBitIoFinishedError;
	   break;
	}

	return sBitIoState;
}

static void write_bits(void)
{
    enum OwBitIoState status = write_bit();

    switch(status)
    {
        case eOwBitIoFinishedError:
            sState = eOwIdle;
          break;
        case eOwBitIoFinishedOk:
            ++sBitidx;
            if(sBitidx >= gOwBitsCount)
            {
                send_status(eOwWriteBitsOk);
                sState = eOwIdle;
            }
          break;
        default:
            if(sStrongPowerRequest == eOwStrongPowerReq)
                if(sBitidx == gOwBitsCount-1)
                    sStrongPowerRequest = eOwStrongPowerKeepAfterWrite;
          break;
    }
}

static void read_bits(void)
{
    enum OwBitIoState status = read_bit();

    switch(status)
    {
        case eOwBitIoFinishedError:
            send_status(eOwReadBitsFailure);
            sState = eOwIdle;
          break;
        case eOwBitIoFinishedOk:
            ++sBitidx;
            if(sBitidx >= gOwBitsCount)
            {
                send_status_with_data(eOwReadBitsOk);
                sState = eOwIdle;
            }
          break;
        default:
          break;
    }
}

void ow_read_bits()
{
	sBitidx = 0;
	sState = eOwReadBits;
}

void ow_write_bits(int strong_power_req)
{
    sBitidx = 0;
    sStrongPowerRequest = strong_power_req ? eOwStrongPowerReq : eOwStrongPower0;
    sState = eOwWriteBits;
}

static inline void storeDirection(int direction)
{
	gOwData.b[1] = direction;
}

static inline int getDirection()
{
	return gOwData.b[1];
}

static inline void setBit2(int v)
{
	unsigned char mask = 1<<2;
	gOwData.b[0] = v ? ( gOwData.b[0] | mask ) : (gOwData.b[0] & ~mask);
}

static void search()
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
		            send_status(eOwReadBitsFailure);
		            sState = eOwIdle;
		          break;
		        case eOwBitIoFinishedOk:
		        	sBitidx++;
		        	if(sBitidx == 2)
		        	{
		        		unsigned v = gOwData.b[0] & 0x3;
		        		if( v == 3 )
		        		{
		        			send_status(eOwSearchResult11);
		        			sState = eOwIdle;
		        		}
		        		else if(v)
		        		{
		        			setBit2(v==1);
		        		}
		        		else
		        		{
		        			setBit2(getDirection());
		        		}
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
		            send_status(eOwWriteBitsFailure);
		            sState = eOwIdle;
		          break;
		        case eOwBitIoFinishedOk:
		        	{
		        		unsigned v = gOwData.b[0] & 0x3;
		        		switch(v){
		        		case 0:
		        			send_status(eOwSearchResult00);
		        			break;
		        		case 1:
		        			send_status(eOwSearchResult0);
		        			break;
		        		case 2:
		        			send_status(eOwSearchResult1);
		        			break;
		        		default:
		        			send_status(eOwSearchResult11);
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

void ow_search(int direction)
{
	storeDirection(direction);
	sBitidx = 0;
	sState = eOwSearch;
}

void ow_init(void)
{
	if(sState != eOwIdle)
	{
		send_status(eRspError);
		return;
	}

	if(bus_active())
	{
		send_status(eOwBusFailure0);
		bus_release();
		return;
	}

	gOwData.param = -1;
	bus_pull();
	sT0 = timer();
	sState = eOwInitWaitResetPulseFallingEdge;
}

void ow_sm_do(void)
{
	switch(sState)
	{
	case eOwIdle:
		break;
	case eOwInitWaitResetPulseFallingEdge:
		wait_reset_pulse_check1();
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
	case eOwInitWaitRecovery:
	    wait_init_recovery();
	    break;
	case eOwReadBits:
		read_bits();
		break;
	case eOwWriteBits:
	    write_bits();
	    break;
	case eOwSearch:
		search();
		break;
	default:
		break;
	}
}

