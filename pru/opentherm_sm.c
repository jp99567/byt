
#include "rpm_iface.h"
#include "common.h"
#include <stdint.h>

//extern void ot_bus_set(int v);
extern int ot_bus(void);
extern void ot_set_bus(int v);
extern unsigned timer(void);
extern void ot_send_frame(enum ResponseCode code, uint32_t data);
extern void send_status(enum ResponseCode code);

enum OtState
{
	eOtIdle,
	eOtWriteFrame,
	eOtFrameWaitGap,
	eOtWaitFrame,
	eOtReadBits,
	eOtFrameErrorWaitGap
};

static enum OtState sState = eOtIdle;
static int sPrevV;
static unsigned sT0;
static unsigned sT0Frame;
static int sBitidx;
static uint32_t sValue;

#define US2TICK(us) ((us)*200)

#define cDurErrorGap US2TICK(50000)
#define cDurErrorFrameTimeout US2TICK(150000)
#define cDurGap US2TICK(10000)
#define cDurBit US2TICK(1000)
#define cDurHalfBit (cDurBit/2)
#define cDurBitCaptureMin ( cDurBit - US2TICK(100) )
#define cDurBitCaptureMax ( cDurBit + US2TICK(150) )

static int timeout(unsigned delay)
{
	return (timer() - sT0) > delay;
}

static int timeout_since(unsigned delay, unsigned t0)
{
	return (timer() - t0) > delay;
}

void ot_transmit(uint32_t data)
{
	if(sState != eOtIdle)
	{
		send_status(eRspError);
		return;
	}
	sState = eOtWriteFrame;
	sValue = data;
	ot_set_bus(1);
	sT0 = timer();
	sBitidx = -1;
}

void ot_sm_do(void)
{
	switch(sState)
	{
	case eOtIdle:
		break;
	case eOtWriteFrame:
	{
		int cur = ( 0 <= sBitidx && sBitidx < 32 )
				? (sValue & (1<<sBitidx) ? 1 : 0)
				: 1;

		if(timeout(cDurBit)){
			ot_set_bus(!cur);
			sT0 = timer();
			if( ++sBitidx > 32 ){
				sState = eOtFrameWaitGap;
			}
		}
		else if(timeout(cDurHalfBit)){
			ot_set_bus(cur);
		}
	}
		break;
	case eOtWaitFrame:
	    if(ot_bus()){
	        sT0 = timer();
	        sT0Frame = sT0;
	        sBitidx = 32;
	        sT0 -= cDurHalfBit;
	        sPrevV = 1;
	        sValue = 0;
	        sState = eOtReadBits;
	    }
		break;
	case eOtReadBits:
		if( sPrevV != ot_bus()){
		    if(timeout(cDurBitCaptureMin)){
		        sT0 = timer();
		        if(sBitidx == -1 || sBitidx == 32){
		            if(!sPrevV){
		                sState = eOtFrameErrorWaitGap;
		            }
		            else if(sBitidx == -1){
		            	ot_send_frame(eOtOk, sValue);
		                sState = eOtIdle;
		            }
		        }
		        else if(sBitidx < 32){
		        	if(sPrevV)
		        		sValue |= (1 << sBitidx);
		        }
		        --sBitidx;
		    }
		    sPrevV = !sPrevV;
		}
		else if(timeout(cDurBitCaptureMax)){
		    sState = eOtFrameErrorWaitGap;
		}
		break;
	case eOtFrameErrorWaitGap:
		if( sPrevV != ot_bus()){
			sT0 = timer();
			sPrevV = !sPrevV;
		}
		else if( ot_bus() && timeout(cDurErrorGap)){
			ot_send_frame(eOtFrameError, sT0-sT0Frame);
			sState = eOtIdle;
		}
		else if(timeout_since(cDurErrorFrameTimeout, sT0Frame)){
			ot_send_frame(eOtBusError, 0);
			sState = eOtIdle;
		}
		break;
	case eOtFrameWaitGap:
	    if(timeout(cDurGap)){
	        sState = eOtWaitFrame;
	    }
	    break;
	default:
		break;
	}
}

