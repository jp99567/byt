
#include "rpm_iface.h"
#include "common.h"

extern void bus_pull(void);
extern void bus_release(void);
extern void bus_power_strong(void);
extern int bus_active(void);
extern unsigned timer(void);
extern void send_status(enum OwResponse code);
extern void send_status_with_data(enum OwResponse code);
extern void send_status_with_param(enum OwResponse code);
extern int gOwBitsCount;
extern union OwData gOwData;

void ow_init(void);
void ow_search(void);
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
	eOwWriteBits
};

enum OwBitIoState
{
    eOwBitIoFinishedOk,
    eOwBitIoFinishedError,
	eOwBitIoInitPulseFallingEdge,
	eOwBitReadPulse,
	eOwBitIoWaitSampleEvent,
	eOwBitIoWaitBusRelaxed,
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
static unsigned sT1;
static enum OwStrongPowerRequest sStrongPowerRequest;
static enum OwBitIoState sBitIoState = eOwBitIoFinishedOk;
static int sBitidx;

#define US2TICK(us) ((us)*20)

#define cDurFallingEdge US2TICK(2)
#define cDurResetPulse US2TICK(480)
#define cDurWaitPresenceT (cDurResetPulse + US2TICK(60))
#define cDurPresenceMinT US2TICK(15)
#define cDurPresenceMaxT US2TICK(240)
#define cDurInitMasterRx US2TICK(480)
#define cDurReadInitT US2TICK(2)
#define cDurReadSample US2TICK(12)
#define cDurSlot US2TICK(60)
#define cDurRecoveryT US2TICK(1)
#define cDurWrite0Pulse US2TICK(60)
#define cDurWrite1Pulse US2TICK(1)

static int timeout(unsigned delay)
{
	return (timer() - sT0) > delay;
}

static int timeout1(unsigned delay)
{
	return (timer() - sT1) > delay;
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
            sBitIoState = eOwBitIoInitPulseFallingEdge;
         }
       }
       break;
      case eOwBitIoInitPulseFallingEdge:
          if(bus_active())
          {
              sT0 = timer();
              sBitIoState = eOwBitIoWaitSampleEvent;
          }
          else if(timeout(cDurFallingEdge))
          {
              sBitIoState = eOwBitIoFinishedError;
              bus_release();
          }
       break;
      case eOwBitIoWaitSampleEvent:
       {
          if( timeout( bitval()
             ? cDurReadInitT
             : cDurReadInitT ) )
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
              sBitIoState = eOwBitIoWaitBusRelaxed;
          }
       }
       break;
      case eOwBitIoWaitBusRelaxed:
       {
           if(bus_active())
           {
               if(timeout(cDurSlot))
                   sBitIoState = eOwBitIoFinishedError;
           }
           else
           {
               sBitIoState = eOwBitIoWaitSlotEnd;
               sT1 = timer();
           }
       }
       break;
      case eOwBitIoWaitSlotEnd:
       {
           if(timeout(cDurSlot) && timeout1(cDurRecoveryT))
               sBitIoState = eOwBitIoFinishedOk;
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
			sBitIoState = eOwBitIoInitPulseFallingEdge;
		 }
	   }
	   break;
	  case eOwBitIoInitPulseFallingEdge:
	   {
	       if(bus_active())
	       {
	           sT0 = timer();
	           sBitIoState = eOwBitReadPulse;
	       }
	       else if(timeout(cDurFallingEdge))
	       {
	           bus_release();
	           sBitIoState = eOwBitIoFinishedError;
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
			   sBitIoState = eOwBitIoWaitBusRelaxed;
		   }
	   }
	   break;
	  case eOwBitIoWaitBusRelaxed:
	   {
		   if(bus_active())
		   {
			   if(timeout(cDurSlot))
				   sBitIoState = eOwBitIoFinishedError;
		   }
		   else
		   {
			   sBitIoState = eOwBitIoWaitSlotEnd;
			   sT1 = timer();
		   }
	   }
	   break;
	  case eOwBitIoWaitSlotEnd:
	   {
		   if(timeout(cDurSlot) && timeout1(cDurRecoveryT))
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
            send_status(eOwWriteBitsFailure);
            sState = eOwIdle;
          break;
        case eOwBitIoFinishedOk:
            if(sBitidx < gOwBitsCount)
            {
                ++sBitidx;
            }
            else
            {
                send_status_with_data(eOwWriteBitsOk);
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
            if(sBitidx < gOwBitsCount)
            {
                ++sBitidx;
            }
            else
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

void ow_search(void)
{

}

void ow_init(void)
{
	if(sState != eOwIdle)
		return;

	if(bus_active())
	{
		send_status(eOwBusFailure0);
		bus_release();
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
	default:
		break;
	}
}

