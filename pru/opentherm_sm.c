
#include "rpm_iface.h"
#include "common.h"
#include <stdint.h>

//extern void ot_bus_set(int v);
extern int ot_bus15(void);
extern int ot_bus14(void);
extern unsigned timer(void);
extern void ot_send_frame(unsigned mark, unsigned duration, unsigned transitions, unsigned value);

enum OtState
{
	eOtDisabled,
	eOtWaitFrame,
	eOtReadBits,
	eOtFrameErrorWaitGap,
	eOtFrameWaitGap
};

struct OtLink
{
    int(*bus)(void);
    unsigned mark_good;
    unsigned mark_bad;
};

struct OtLink sLink15 = {&ot_bus15, 0x15AAABCD, 0xEBAD0015};
struct OtLink sLink14 = {&ot_bus14, 0x14AAABCD, 0xEBAD0014};
struct OtLink* spLink = &sLink15;

void swapLink()
{
    spLink = spLink == &sLink15 ? &sLink14 : &sLink15;
}

static enum OtState sState = eOtDisabled;
static int sPrevV;
static unsigned sT0;
static unsigned sT0Frame;
static int sBitidx;
static unsigned sValue;
static int sTransitionNr;

#define US2TICK(us) ((us)*200)

#define cDurErrorGap US2TICK(500000)
#define cDurGap US2TICK(10000)
#define cDurBit US2TICK(1000)
#define cDurBitCaptureMin ( cDurBit - US2TICK(100) )
#define cDurBitCaptureMax ( cDurBit + US2TICK(150) )

static int timeout(unsigned delay)
{
	return (timer() - sT0) > delay;
}

void ot_enable(void)
{
	if(sState == eOtDisabled)
		sState = eOtWaitFrame;
}

void ot_sm_do(void)
{
	switch(sState)
	{
	case eOtDisabled:
		break;
	case eOtWaitFrame:
	    if(spLink->bus()){
	        sT0 = timer();
	        sT0Frame = sT0;
	        sBitidx = 32;
	        sT0 -= cDurBit / 2;
	        sPrevV = 1;
	        sTransitionNr = 1;
	        sValue = 0;
	        sState = eOtReadBits;
	    }
		break;
	case eOtReadBits:
		if( sPrevV != spLink->bus()){
		    if(timeout(cDurBitCaptureMin)){
		        sT0 = timer();
		        if(sBitidx == -1 || sBitidx == 32){
		            if(!sPrevV){
		                sState = eOtFrameErrorWaitGap;
		            }
		            else if(sBitidx == -1){
		            	ot_send_frame(spLink->mark_good, sT0-sT0Frame, sTransitionNr+1, sValue);
		                sState = eOtFrameWaitGap;
		            }
		        }
		        else if(sBitidx < 32){
		        	if(sPrevV)
		        		sValue |= (1 << sBitidx);
		        }
		        --sBitidx;
		    }
		    ++sTransitionNr;
		    sPrevV = !sPrevV;
		}
		else if(timeout(cDurBitCaptureMax)){
		    sState = eOtFrameErrorWaitGap;
		}
		break;
	case eOtFrameErrorWaitGap:
		if( sPrevV != spLink->bus()){
			sT0 = timer();
			++sTransitionNr;
			sPrevV = !sPrevV;
		}
		else if( !spLink->bus() && timeout(cDurErrorGap)){
			ot_send_frame(spLink->mark_bad, timer()-sT0Frame, sTransitionNr, sBitidx);
			swapLink();
			sState = eOtWaitFrame;
		}
		break;
	case eOtFrameWaitGap:
	    if(timeout(cDurGap)){
	        swapLink();
	        sState = eOtWaitFrame;
	    }
	    break;
	default:
		break;
	}
}

