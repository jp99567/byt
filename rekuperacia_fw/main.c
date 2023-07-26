
// PIC18F6720 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1H
#pragma config OSC = HS         // Oscillator Selection bits (HS oscillator)
#pragma config OSCS = OFF       // Oscillator System Clock Switch Enable bit (Oscillator system clock switch option is disabled (main oscillator is source))

// CONFIG2L
#pragma config PWRT = OFF       // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOR = ON         // Brown-out Reset Enable bit (Brown-out Reset enabled)
#pragma config BORV = 42        // Brown-out Reset Voltage bits (VBOR set to 4.2V)

// CONFIG2H
#pragma config WDT = ON         // Watchdog Timer Enable bit (WDT enabled)
#pragma config WDTPS = 128      // Watchdog Timer Postscale Select bits (1:128)

// CONFIG3L

// CONFIG3H
#pragma config CCP2MUX = ON     // CCP2 Mux bit (CCP2 input/output is multiplexed with RC1)

// CONFIG4L
#pragma config STVR = ON        // Stack Full/Underflow Reset Enable bit (Stack full/underflow will cause Reset)
#pragma config LVP = OFF        // Low-Voltage ICSP Enable bit (Low-voltage ICSP disabled)

// CONFIG5L
#pragma config CP0 = OFF        // Code Protection bit (Block 0 (000200-003FFFh) not code-protected)
#pragma config CP1 = OFF        // Code Protection bit (Block 1 (004000-007FFFh) not code-protected)
#pragma config CP2 = OFF        // Code Protection bit (Block 2 (008000-00BFFFh) not code-protected)
#pragma config CP3 = OFF        // Code Protection bit (Block 3 (00C000-00FFFFh) not code-protected)
#pragma config CP4 = OFF        // Code Protection bit (Block 4 (010000-013FFFh) not code-protected)
#pragma config CP5 = OFF        // Code Protection bit (Block 5 (014000-017FFFh) not code-protected)
#pragma config CP6 = OFF        // Code Protection bit (Block 6 (018000-01BFFFh) not code-protected)
#pragma config CP7 = OFF        // Code Protection bit (Block 7 (01C000-01FFFFh) not code-protected)

// CONFIG5H
#pragma config CPB = OFF        // Boot Block Code Protection bit (Boot Block (000000-0001FFh) not code-protected)
#pragma config CPD = OFF        // Data EEPROM Code Protection bit (Data EEPROM not code-protected)

// CONFIG6L
#pragma config WRT0 = OFF       // Write Protection bit (Block 0 (000200-003FFFh) not write-protected)
#pragma config WRT1 = OFF       // Write Protection bit (Block 1 (004000-007FFFh) not write-protected)
#pragma config WRT2 = OFF       // Write Protection bit (Block 2 (008000-00BFFFh) not write-protected)
#pragma config WRT3 = OFF       // Write Protection bit (Block 3 (00C000-00FFFFh) not write-protected)
#pragma config WRT4 = OFF       // Write Protection bit (Block 4 (010000-013FFFh) not write-protected)
#pragma config WRT5 = OFF       // Write Protection bit (Block 5 (014000-017FFFh) not write-protected)
#pragma config WRT6 = OFF       // Write Protection bit (Block 6 (018000-01BFFFh) not write-protected)
#pragma config WRT7 = OFF       // Write Protection bit (Block 7 (01C000-01FFFFh) not write-protected)

// CONFIG6H
#pragma config WRTC = OFF       // Configuration Register Write Protection bit (Configuration registers (300000-3000FFh) not write-protected)
#pragma config WRTB = OFF       // Boot Block Write Protection bit (Boot Block (000000-0001FFh) not write-protected)
#pragma config WRTD = OFF       // Data EEPROM Write Protection bit (Data EEPROM not write-protected)

// CONFIG7L
#pragma config EBTR0 = OFF      // Table Read Protection bit (Block 0 (000200-003FFFh) not protected from table reads executed in other blocks)
#pragma config EBTR1 = OFF      // Table Read Protection bit (Block 1 (004000-007FFFh) not protected from table reads executed in other blocks)
#pragma config EBTR2 = OFF      // Table Read Protection bit (Block 2 (008000-00BFFFh) not protected from table reads executed in other blocks)
#pragma config EBTR3 = OFF      // Table Read Protection bit (Block 3 (00C000-00FFFFh) not protected from table reads executed in other blocks)
#pragma config EBTR4 = OFF      // Table Read Protection bit (Block 4 (010000-013FFFh) not protected from table reads executed in other blocks)
#pragma config EBTR5 = OFF      // Table Read Protection bit (Block 5 (014000-017FFFh) not protected from table reads executed in other blocks)
#pragma config EBTR6 = OFF      // Table Read Protection bit (Block 6 (018000-01BFFFh) not protected from table reads executed in other blocks)
#pragma config EBTR7 = OFF      // Table Read Protection bit (Block 7 (01C000-01FFFFh) not protected from table reads executed in other blocks)

// CONFIG7H
#pragma config EBTRB = OFF      // Boot Block Table Read Protection bit (Boot Block (000000-0001FFh) not protected from table reads executed in other blocks)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF
/* Low voltage programming enabled , RE3 pin is MCLR */


#include <xc.h>
#include <stdint.h>
#include <string.h>

uint8_t calc_crc(uint8_t b, uint8_t prev_b);

const uint8_t mark_mask = 0xFC;
const uint8_t ctrl_bypass = 1<<0;
const uint8_t ctrl_off = 1<<1;

struct {
    uint8_t ctrl;
    uint8_t pwmIntake;
    uint8_t pwmExhaust;
    uint8_t crc;
} rekuRx;

struct VentCh {
    uint16_t period;
    uint16_t temp;
};

struct VentCh intake;
struct VentCh exhaust;
    
const uint8_t markCmd = 0x54;
const uint8_t markCnf = 0x8C;
struct {
    uint8_t stat;
    struct VentCh intake;
    struct VentCh exhaust;
    uint8_t crc;
} rekuTx;

#define MS2TICK(ms) ((ms)*5)
uint16_t sT0;
int timeoutT0(uint16_t delay)
{
        return (TMR0 - sT0) > delay;
}

void checkRxErr()
{
    if(RCSTA1 & 6){
        do{
            RCSTA1bits.CREN = 0;
        }
        while(RCSTA1 & 6);
        RCSTA1bits.CREN = 1;
    }
}

void init_usart()
{
    TRISC7 = 1;
    TRISC6 = 0;
    SPBRG1 = 7; //9600Baud
    RCSTA1bits.SPEN = 1;
    RCSTA1bits.CREN = 1;
    TXSTA1bits.TXEN = 1;
}

uint8_t reset_condition_flags;
void handle_reset_condition()
{
    reset_condition_flags = RCON & 0x1F;
    if(STKFUL)
        reset_condition_flags |= (1<<7);
    if(STKOVF)
        reset_condition_flags |= (1<<6);
    RCONbits.RI = 1;
    RCONbits.POR = 1;
    RCONbits.BOR = 1;
}

int is_power_on_reset()
{
    return 0x1C == (reset_condition_flags & 0x9F);
}

enum CommState { NO_COMM, COMM, COMM_LOST };
uint8_t commState;

enum TxCommState { COMMTX_IDLE, COMMTX_PENDING };
struct {
    enum TxCommState state;
    int byte_idx;
}txCtx;

void do_tx()
{
    switch(txCtx.state){
        case COMMTX_IDLE:
          break;
        case COMMTX_PENDING:
        {
            if(txCtx.byte_idx < sizeof(rekuTx)){
                if(TX1IF == 0){
                    uint8_t* data = (uint8_t*)&rekuTx;
                    TXREG1 = data[txCtx.byte_idx++];
                }
                else{
                    if(TXSTA1bits.TRMT){
                        txCtx.state = COMMTX_IDLE;
                        CLRWDT();
                    }
                }
            }
        }
          break;
        default:
          break;
    }
}

void start_tx()
{
    if(txCtx.state != COMMTX_IDLE)
        return;
    
    
    rekuTx.exhaust = exhaust;
    rekuTx.intake = intake;
    rekuTx.intake.temp |= (reset_condition_flags & 0x1F);
    rekuTx.exhaust.temp |= (reset_condition_flags >> 6);
    
    uint8_t* data = (uint8_t*)&rekuTx;
    uint8_t crc = 0;
    for(uint8_t i=0; i<sizeof(rekuTx)-1; i++)
        crc = calc_crc(data[i], crc);
    rekuTx.crc = crc;
    txCtx.byte_idx = 0;
    txCtx.state = COMMTX_PENDING;
}

enum RxCommState { COMMRX_READING, COMMRX_ERR };
struct {
    enum TxCommState state;
    int byte_idx;
}rxCtx;
void do_rx()
{
            if(RC1IF){
                uint8_t* data = (uint8_t*)&rekuRx;
            do{
                data[rxCtx.byte_idx++] = RCREG1;
                checkRxErr();
            }
            while(RC1IF);
        }
            else{
            
    switch(rxCtx.state)
    {
        case COMMRX_READING:
           break;
        default:
           break;
    }
            }
}

static void init()
{
    handle_reset_condition();
    //ADCON0bits.ADON = 1;
    //CCP1CONbits.CCP1M = 0xC;
    TRISD7 = 0; // PCB orange LED
    
    //USART1
    
    
    //4.9152MHz
    T0CON = 0;
    TMR0ON = 1;
    T0CON |= 7; //1:256 T=3.653s dT=208us
//    TRISC1_bit = 0;
//    TRISC2_bit = 0;
    
    rekuTx.stat = markCnf;
    
    init_usart();
    if(!is_power_on_reset())
        rxCtx.state = COMMRX_ERR;
}

void main(void)
{
    init();
    sT0 = TMR0;
    while(1) 
    {
        do_tx();
        
        if(TMR0 > 0xC00){
            PORTDbits.RD7 = 1;
        }
        else{
            PORTDbits.RD7 = 0;
        }
        
        do_rx();
    }
}

const uint8_t __attribute__((space(prog))) one_wire_crc8_table[] = {
 0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
 157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
 35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
 190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
 70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
 219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
 101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
 248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
 140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
 17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
 175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
 50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
 202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
 87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
 233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
 116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

uint8_t calc_crc(uint8_t b, uint8_t prev_b)
{
	return one_wire_crc8_table[prev_b ^ b];
}

/*************************************************************
 
 2x17 conn J1 J10 J11
A1,B1 24V
A2,B2 Vdd
A3,B3 GND
B13 summer bypass rele (Q1b 2k2) -- RC0
B15 - J5.1 temp.sens. -- AN1
B14 - J5.2 temp.sens. --- AVDD
B17 - J4.1 temp.sens. -- AN0
B16 - J4.2 temp.sens. --- AVDD
A7 - 2k2 vdd pullup
B8 - J7.1
B7 - J7.2 -- RC1
B6 - J7.3
B5 - J7.4 -- RB4
B12 - J6.1
B11 - J6.2 -- RC2
B10 - J6.3
B9 - J6.4 -- RB5

J6 intake fan
J7 exhaust fan
1 - cerveny - 10V
2 - zlty - control input
3 - modry - gnd
4 - biely - tach

1.2KHz 42.5% 18.8Hz
100% 41.7Hz

nLED - RD7
 
**************************************************************/
