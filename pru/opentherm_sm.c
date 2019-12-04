
#include "rpm_iface.h"
#include "common.h"
#include <stdint.h>

//extern void ot_bus_set(int v);
extern int ot_bus(void);
extern unsigned timer(void);
extern void ot_send_error_frame(unsigned duration, unsigned transitions, unsigned bitread);
extern void ot_send_frame(unsigned duration, unsigned transitions, unsigned value);

enum OtState
{
	eOtWaitFrame,
	eOtReadBits,
	eOtFrameErrorWaitGap,
};

static enum OtState sState = eOtWaitFrame;
static int sPrevV;
static unsigned sT0;
static unsigned sT0Frame;
static int sBitidx;
static unsigned sValue;
static int sTransitionNr;

#define US2TICK(us) ((us)*200)

#define cDurErrorGap US2TICK(10000)
#define cDurBit US2TICK(1000)
#define cDurBitCaptureMin ( cDurBit - US2TICK(100) )
#define cDurBitCaptureMax ( cDurBit + US2TICK(150) )

static int timeout(unsigned delay)
{
	return (timer() - sT0) > delay;
}

void ot_sm_do(void)
{
	switch(sState)
	{
	case eOtWaitFrame:
	    if(ot_bus()){
	        sT0 = timer();
	        sT0Frame = sT0;
	        sBitidx = -1;
	        sT0 -= cDurBit / 2;
	        sPrevV = 1;
	        sTransitionNr = 1;
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
		            else if(sBitidx == 32){
		            	ot_send_frame(sT0-sT0Frame, sTransitionNr+1, sValue);
		                sState = eOtWaitFrame;
		            }
		        }
		        else if(sBitidx < 32){
		        	if(sPrevV)
		        		sValue |= (1 << sBitidx);
		        }
		        ++sBitidx;
		    }
		    ++sTransitionNr;
		    sPrevV = !sPrevV;
		}
		else if(timeout(cDurBitCaptureMax)){
		    sState = eOtFrameErrorWaitGap;
		}
		break;
	case eOtFrameErrorWaitGap:
		if( sPrevV != ot_bus()){
			sT0 = timer();
			++sTransitionNr;
			sPrevV = !sPrevV;
		}
		else if( !ot_bus() && timeout(cDurErrorGap)){
			ot_send_error_frame(timer()-sT0Frame, sTransitionNr, sBitidx);
			sState = eOtWaitFrame;
		}
		break;
	default:
		break;
	}
}

