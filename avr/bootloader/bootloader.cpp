#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

constexpr uint8_t u8min(uint8_t a, uint8_t b){
	return a < b ? a : b;
}

#ifndef SLAVE_SERVICE_ID
#error "slave id not defined"
#endif

constexpr uint32_t base_can_bcast = 0xEC33F000;
constexpr uint32_t base_can_id = base_can_bcast|(SLAVE_SERVICE_ID<<5);
constexpr uint32_t base_can_id_tx = base_can_id | (1<<3);
//static_assert(base_can_id == 0x4c33f300+2) && base_can_id < (0x4c33f300+64), "wrong base can id");
//static_assert(base_can_id >= (0x4c33f300+1) && base_can_id < (0x4c33f300+64), "wrong base can id");

static uint8_t msg[8] __attribute__ ((section (".noinit")));
static uint8_t msglen __attribute__ ((section (".noinit")));
static uint8_t page[SPM_PAGESIZE] __attribute__ ((section (".noinit")));
static uint8_t page_in8b __attribute__ ((section (".noinit")));

struct PageAddr
{
	uint8_t cmd;
	uint32_t addr;
}__attribute__((__packed__));

bool can_rx()
{
	for(uint8_t id=13; id<=14; ++id){
		CANPAGE = ( id << MOBNB0 );
		if(CANSTMOB & _BV(RXOK)){
			msglen = u8min(CANCDMOB & 0x0F, 8);

			for(uint8_t i=0; i<msglen; ++i)
				msg[i] = CANMSG;
			CANSTMOB = 0;
			CANCDMOB = ( 1 << CONMOB1) | ( 1 << IDE );
			if(msglen > 0)
				return true;
		}
	}
	return false;
}

void can_tx(void) {

	CANPAGE = ( 12 << MOBNB0 );
	while ( CANEN1 & ( 1 << ENMOB12 ) ); // Wait for MOb 0 to be free
	CANSTMOB = 0x00;    	// Clear mob status register
	for(uint8_t i=0; i<msglen; ++i)
		CANMSG = msg[i];

	CANCDMOB = ( 1 << CONMOB0 ) | ( 1 << IDE ) | ( msglen << DLC0 ); 	// Enable transmission, data length=1 (CAN Standard rev 2.0B(29 bit identifiers))
	while ( ! ( CANSTMOB & ( 1 << TXOK ) ) );	// wait for TXOK flag set
	CANCDMOB = 0x00;
	CANSTMOB = 0x00;
}

void boot_program_page (uint32_t pageidx, uint8_t *buf)
{
    uint16_t i;

    eeprom_busy_wait ();
    boot_page_erase (pageidx);
    boot_spm_busy_wait ();      // Wait until the memory is erased.
    for (i=0; i<SPM_PAGESIZE; i+=2)
    {
        // Set up little-endian word.
        uint16_t w = *buf++;
        w += (*buf++) << 8;

        boot_page_fill (pageidx + i, w);
    }
    boot_page_write (pageidx);     // Store buffer in flash page.
    boot_spm_busy_wait();       // Wait until the memory is written.
    // Reenable RWW-section again. We need this if we want to jump back
    // to the application after bootloading.
    boot_rww_enable();
}

//***** CAN ialization *****************************************************
void can_init(void){

	CANGCON = ( 1 << SWRES );   // Software reset
	CANTCON = 255;
	CANBT1 = 0x08;   // set baud rate to 100kb
	CANBT2 = 0x0C;
	CANBT3 = 0x37;
	for ( int8_t mob=0; mob<15; mob++ ) {
		CANPAGE = ( mob << MOBNB0 );
		CANCDMOB = 0x00;       		// Disable mob
		CANSTMOB = 0x00;     		// Clear mob status register;
	}

	CANPAGE = ( 13 << MOBNB0 );
	CANIDM = (0xFFFFFFFF << 3)|1;
	CANIDT = base_can_id;
	CANCDMOB = ( 1 << CONMOB1) | ( 1 << IDE );

	CANPAGE = ( 14 << MOBNB0 );
	CANIDM = (0xFFFFFFFF << 3)|1;
	CANIDT = base_can_bcast;
	CANCDMOB = ( 1 << CONMOB1) | ( 1 << IDE );

	CANPAGE = ( 12 << MOBNB0 );
	CANIDT = base_can_id_tx;
	CANGCON |= ( 1 << ENASTB );
}

void init()
{
	can_init();
}

int main() {
	bool bootloader_finished = false;
	bool timeout_disabled = false;
	bool downloading = false;
	uint8_t xorsum;

	init();

	while(!bootloader_finished){
		if(can_rx()){
			if(downloading){
				for(uint8_t i=0; i<8; ++i){
					page[8*page_in8b++ + i] = msg[i];
					xorsum ^= msg[i];
				}
				if(page_in8b == SPM_PAGESIZE/8){
					downloading = false;
					msglen = 2;
					msg[0] = 'D';
					msg[1] = xorsum;
				}
			}
			else {
				switch(msg[0])
				{
				case 'h':
					set_sleep_mode(_BV(SM1)|_BV(SE)); //power down mode
					do {sleep_cpu(); } while(1);
					break;
				case 'c':
					msglen = 1;
					msg[0] = 'C';
					bootloader_finished = true;
					break;
				case 's':
					msglen = 4;
					msg[0] = 'S';
					msg[1] = CANTEC;
					msg[2] = CANREC;
					msg[3] = MCUSR;
					break;
				case 'f':
				{
					PageAddr* p = (PageAddr*)msg;
					boot_program_page(p->addr, page);
					msglen = 1;
					msg[0] = 'F';
				}
					break;
				case 'd':
					downloading = true;
					page_in8b = 0;
					xorsum = 0xA5;
					break;
				default:
					break;
				}
				timeout_disabled = true;
				if(not downloading)
					can_tx();
			}
		}

		if(not timeout_disabled){
			if(CANGIT & _BV(OVRTIM)){
				CANGIT |= _BV(OVRTIM);
				bootloader_finished = true;
			}
		}
	}
	asm("jmp 0000");
	return 0;
}
