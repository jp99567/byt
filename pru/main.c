/*
 * Copyright (C) 2016 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the
 *	  distribution.
 *
 *	* Neither the name of Texas Instruments Incorporated nor the names of
 *	  its contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdio.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <pru_ctrl.h>
#include <rsc_types.h>
#include <pru_iep.h>
#include <pru_rpmsg.h>
#include "resource_table.h"
#include "intc_map_0.h"

volatile register uint32_t __R31;
volatile register uint32_t __R30;

/* Host-0 Interrupt sets bit 30 in register R31 */
/* Host-1 Interrupt sets bit 31 in register R31 */
#define HOST_INT			((uint32_t) 1 << 30)

/* The PRU-ICSS system events used for RPMsg are defined in the Linux device tree
 * PRU0 uses system event 16 (To ARM) and 17 (From ARM)
 * PRU1 uses system event 18 (To ARM) and 19 (From ARM)
 */
#define TO_ARM_HOST			16
#define FROM_ARM_HOST			17

/*
 * Using the name 'rpmsg-pru' will probe the rpmsg_pru driver found
 * at linux-x.y.z/drivers/rpmsg/rpmsg_pru.c
 */
#define CHAN_NAME			"rpmsg-pru"
#define CHAN_PORT			30


/*
 * Used to make sure the Linux drivers are ready for RPMsg communication
 * Found at linux-x.y.z/include/uapi/linux/virtio_config.h
 */
#define VIRTIO_CONFIG_S_DRIVER_OK	4

uint8_t payload[RPMSG_MESSAGE_SIZE];

void eip_timer_init(void)
{
    /* Disable counter */
    CT_IEP.TMR_GLB_CFG_bit.CNT_EN = 0;

    /* Reset Count register */
    CT_IEP.TMR_CNT = 0x0;

    /* Clear overflow status register */
    CT_IEP.TMR_GLB_STS_bit.CNT_OVF = 0x1;

    /* Clear compare status */
    CT_IEP.TMR_CMP_STS_bit.CMP_HIT = 0xFF;

    /* Disable compensation */
    CT_IEP.TMR_COMPEN_bit.COMPEN_CNT = 0x0;

    /* Clear the status of all interrupts */
    CT_INTC.SECR0 = 0xFFFFFFFF;
    CT_INTC.SECR1 = 0xFFFFFFFF;

    /* Enable counter */
    CT_IEP.TMR_GLB_CFG = 0x11;
}

unsigned timer(void)
{
    return CT_IEP.TMR_CNT;
}

struct Request
{
    int32_t cmd;
    uint32_t data[];
};

struct pru_rpmsg_transport transport;
uint16_t src, dst;

///////////////////////////////////////// BEGIN OW /////////////////////////////////
#define OW_OUTPIN 1       // pru0_r30_out1
#define OW_OUTPIN_POWER 0 // pru0_r30_out0
#define OW_INPIN 5        // pru0_r31_in5

#include "rpm_iface.h"
#include "common.h"

struct OwResponseWithData
{
    uint32_t code;
    uint8_t data[];
};

struct OwResponseWithParam
{
    uint32_t code;
    int32_t param;
};

int gOwBitsCount;
union OwData gOwData;

void send_status(enum ResponseCode code)
{
    int32_t v = code;
    pru_rpmsg_send(&transport, dst, src, &v, sizeof(v));
}

void send_status_with_data(enum ResponseCode code)
{
    struct OwResponseWithData* p = (struct OwResponseWithData*)payload;
    p->code = code;
    uint16_t len = 1+(gOwBitsCount-1)/8;
    memcpy(p->data, gOwData.b, len);
    pru_rpmsg_send(&transport, dst, src, payload, sizeof(*p) + len);
}

void send_status_with_param(enum ResponseCode code)
{
    struct OwResponseWithParam p;
    p.code = code;
    p.param = gOwData.param;
    pru_rpmsg_send(&transport, dst, src, &p, sizeof(p));
}

void bus_pull(void)
{
    __R30 &= ~(1<<OW_OUTPIN_POWER);
    __R30 |= 1<<OW_OUTPIN;
}

void bus_power_strong(void)
{
    __R30 &= ~(1<<OW_OUTPIN);
    __R30 |= 1<<OW_OUTPIN_POWER;
}

void bus_release(void)
{
    __R30 &= ~( (1<<OW_OUTPIN) | (1<<OW_OUTPIN_POWER) );
}

int bus_active (void)
{
    return __R31 & (1<<OW_INPIN) ? 0 : 1;
}

extern void ow_init(void);
extern void ow_read_bits(void);
extern void ow_write_bits(int strong_power_req);
extern void ow_search(int direction);
extern void ow_sm_do(void);
///////////////////////////////// END OW ////////////////////////////////////////////

////////////////////////////////// OT ///////////////////////////////////////////////
#define OT_INPIN 14 // pru0_in14
#define OT_OUTPIN 14 // pru0_out14
int ot_bus(void)
{
    return __R31 & (1<<OT_INPIN) ? 0 : 1;
}

void ot_set_bus(int v)
{
	if(v)
	    __R30 &= ~(1<<OT_OUTPIN);
	else
	    __R30 |= 1<<OT_OUTPIN;
}

void ot_send_frame(enum ResponseCode code, uint32_t data)
{
	uint32_t* u32Array = (uint32_t*)payload;
	u32Array[0] = code;
	u32Array[1] = data;
	pru_rpmsg_send(&transport, dst, src, payload, sizeof(uint32_t)*2);
}

extern void ot_sm_do(void);
extern void ot_transmit(uint32_t data);
///////////////////////////////// END OT ////////////////////////////////////////////



void process_message(void* data, uint16_t len)
{
    struct Request* req = data;

    switch(req->cmd)
    {
    case eCmdOwInit:
        if(len==sizeof(req->cmd))
        {
            ow_init();
            return;
        }
      break;
    case eCmdOwSearchDir0:
    case eCmdOwSearchDir1:
        if(len==sizeof(req->cmd))
         {
            ow_search(req->cmd == eCmdOwSearchDir1);
            return;
         }
      break;
    case eCmdOwRead:
        if(len == sizeof(req->cmd) + sizeof(req->data[0]))
        {
            gOwBitsCount = req->data[0];
            if(gOwBitsCount > 0)
            {
                int32_t bytelen = (gOwBitsCount-1)/8 + 1;
                if(bytelen <= sizeof(gOwData.b))
                {
                    ow_read_bits();
                    return;
                }
            }
        }
      break;
    case eCmdOwWrite:
    case eCmdOwWritePower:
        if(len > sizeof(req->cmd) + sizeof(req->data[0]))
        {
            gOwBitsCount = req->data[0];
            if(gOwBitsCount > 0)
            {
                int32_t bytelen = (gOwBitsCount-1)/8 + 1;
                if(bytelen <= sizeof(gOwData.b) &&
                   len >= sizeof(req->cmd) + sizeof(req->data[0]) + bytelen)
                {
                    memcpy(gOwData.b, &req->data[1], bytelen);
                    ow_write_bits(req->cmd == eCmdOwWritePower);
                    return;
                }
            }
        }
      break;
    case eCmdOtTransmit:
    	if(len == sizeof(req->cmd) + sizeof(req->data[0])){
    		ot_transmit(req->data[0]);
    		return;
    	}
    	break;
    default:
      break;
    }
    send_status(eRspError);
}

void main(void)
{
	uint16_t len;
	volatile uint8_t *status;

	bus_release();
	ot_set_bus(0);

	/* Allow OCP master port access by the PRU so the PRU can read external memories */
	CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

	eip_timer_init();

	/* Clear the status of the PRU-ICSS system event that the ARM will use to 'kick' us */
	CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

	/* Make sure the Linux drivers are ready for RPMsg communication */
	status = &resourceTable.rpmsg_vdev.status;
	while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));


	/* Initialize the RPMsg transport structure */
	pru_rpmsg_init(&transport, &resourceTable.rpmsg_vring0, &resourceTable.rpmsg_vring1, TO_ARM_HOST, FROM_ARM_HOST);

	/* Create the RPMsg channel between the PRU and ARM user space using the transport structure. */
	while (pru_rpmsg_channel(RPMSG_NS_CREATE, &transport, CHAN_NAME, CHAN_PORT) != PRU_RPMSG_SUCCESS);

	while (1) {

		/* Check if the ARM has kicked us */
		if (__R31 & HOST_INT) {
			/* Clear the event status */
			CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;
			/* Receive all available messages, multiple messages can be sent per kick */
			while(pru_rpmsg_receive(&transport, &src, &dst, payload, &len) == PRU_RPMSG_SUCCESS) {
				process_message(payload, len);
			}
		}

		ow_sm_do();
		ot_sm_do();
	}
}
