/*
 * sm.c
 *
 *  Created on: Sep 11, 2018
 *      Author: pod
 */

enum OwResponse {
	eOwPresenceOk,
	eOwBusFailure0,
	eOwBusFailure1,
	eOwNoPresence,
	eOwBusFailureTimeout,
	eOw
};

void bus_pull(void);
void bus_release(void);
void bus_power_strong(void);
int bus_active(void);
unsigned timer(void);
void send_status(enum OwResponse code);

void ow_init(void);
void ow_read_bits(int count);
void ow_sm_do(void);

enum OwState
{
	eOwIdle,
	eOwWaitResetPulseCheck1,
	eOwWaitResetPulse,
	eOwWaitPresence,
	eOwWaitPresenceCleared,
	eOwReadBits
};

enum OwReadBitState
{
	eOwRead1UsInit,
	eOwReadWaitValue,
	eOwReadWaitBusRelaxed,
	eOwReadWaitSlotEnd,
	eOwReadFinishedOk,
	eOwReadFinishedError
};

static enum OwState gState = eOwIdle;
static unsigned gT0;
static unsigned gT1;
static int gBitsLeft;
static enum OwReadBitState gReadBitState = eOwReadFinishedOk;
static char data[9];
static int bitidx;

#define US2TICK(us) ((us)*20)

#define cDurResetPulseT US2TICK(480)
#define cDurResetPulseCheck1T US2TICK(1)
#define cDurWaitPresenceT (cDurResetPulseT + US2TICK(60))
#define cDurPresenceMinT US2TICK(15)
#define cDurPresenceMaxT US2TICK(240)
#define cDurReadInitT US2TICK(1)
#define cDurReadSample US2TICK(12)
#define cDurSlot US2TICK(60)
#define cDurRecoveryT U2TICK(1)

static int timeout(unsigned delay)
{
	return (timer() - gT0) > delay;
}

static int timeout1(unsigned delay)
{
	return (timer() - gT1) > delay;
}

static void wait_reset_pulse_check1(void)
{
	if(timeout(cDurResetPulseCheck1T))
	{
		if(bus_active())
		{
			gState = eOwWaitResetPulse;
		}
		else
		{
			bus_release();
			send_status(eOwBusFailure1);
			gState = eOwIdle;
		}
	}
}

static void wait_reset_pulse(void)
{
	if(timeout(cDurResetPulseT))
	{
		bus_release();
		gState = eOwWaitPresence;
	}
}

static void wait_presence_pulse(void)
{
	if(timeout(cDurWaitPresenceT))
	{
		gState = eOwWaitPresenceCleared;
		gT0 = timer();
	}
}

static void wait_presence_cleared(void)
{
	if(!bus_active())
	{
		if(timeout(cDurPresenceMinT))
			send_status(eOwPresenceOk);
		else
			send_status(eOwNoPresence);

		gState = eOwIdle;
	}
	else if(timeout(cDurPresenceMaxT))
	{
		send_status(eOwBusFailureTimeout);
		gState = eOwIdle;
	}
}

static void sample(void)
{
	unsigned byte = bitidx / 8;
	unsigned bitmask = 1 << bitidx % 8;

	if( ! bus_active() )
	{
		data[byte] |= bitmask;
	}
	else
	{
		data[byte] &= ~bitmask;
	}
}

static enum OwReadBitState read_bit(void)
{
	switch(gReadBitState)
	{
	  case eOwReadFinishedOk:
	  case eOwReadFinishedError:
	   {
		 if(bus_active())
		 {
			gReadBitState = eOwReadFinishedError;
		 }
		 else
		 {
			bus_pull();
			gT0 = timer();
			gReadBitState = eOwRead1UsInit;
		 }
	   }
	   break;
	  case eOwRead1UsInit:
	   {
		  if(timeout(cDurReadInitT))
		  {
			  bus_release();
			  gReadBitState = eOwReadWaitValue;
		  }
	   }
	   break;
	  case eOwReadWaitValue:
	   {
		   if(timeout(cDurReadSample))
		   {
			   sample();
			   gReadBitState = eOwReadWaitBusRelaxed;
		   }
	   }
	   break;
	  case eOwReadWaitBusRelaxed:
	   {
		   if(bus_active())
		   {
			   if(timeout(cDurSlot))
				   gReadBitState = eOwReadFinishedError;
		   }
		   else
		   {
			   gReadBitState = eOwReadWaitSlotEnd;
			   gT1 = timer();
		   }
	   }
	   break;
	  case eOwReadWaitSlotEnd:
	   {
		   if(timeout(cDurSlot) && timeout1(cDurRecoveryT))
			   gReadBitState = eOwReadFinishedOk;
	   }
	   break;
	  default:
		 gReadBitState = eOwReadFinishedError;
	   break;
	}

	return gReadBitState;
}

static enum OwReadBitState read_bits(void)
{
	Todo
}

void ow_read_bits(int count)
{
	gBitsLeft = count;
	gState = eOwReadBits;
}

void ow_init(void)
{
	if(gState != eOwIdle)
		return;

	if(bus_active())
	{
		send_status(eOwBusFailure0);
	}

	bus_pull();
	gT0 = timer();
	gState = eOwWaitResetPulse;
}

void ow_sm_do(void)
{
	switch(gState)
	{
	case eOwIdle:
		break;
	case eOwWaitResetPulseCheck1:
		wait_reset_pulse_check1();
		break;
	case eOwWaitResetPulse:
		wait_reset_pulse();
		break;
	case eOwWaitPresence:
		wait_presence_pulse();
		break;
	case eOwWaitPresenceCleared:
		wait_presence_cleared();
		break;
	case eOwReadBits:
		read_bits();
		break;
	default:
		break;
	}
}

