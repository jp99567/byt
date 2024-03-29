
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
#include <string.h>
#include "reku_control_proto.h"

uint8_t calc_crc(uint8_t b, uint8_t prev_b);
void letny_bypass(uint8_t on);

struct VentCh rekuCh[2];
struct RekuTx rekuTx;
struct RekuRx rekuRx;

void led_toggle()
{
    PORTDbits.RD7 ^= 1;
}

void led_set()
{
    PORTDbits.RD7 = 0;
}

void updateControlPVs()
{
    uint8_t off = rekuRx.ctrl & ctrl_off;
    uint8_t bypass = rekuRx.ctrl & ctrl_bypass;
    if(off){
        letny_bypass(0);
        CCP1CON &= ~((1<<4)|(1<<5)); //duty bit0 bit1
        CCP2CON &= ~((1<<4)|(1<<5));
        CCPR1L = 0;
        CCPR2L = 0;
    }
    else{
        letny_bypass(bypass);
        CCP1CON |= ((1<<4)|(1<<5)); //duty bit0 bit1
        CCP2CON |= ((1<<4)|(1<<5));
        CCPR1L = rekuRx.pwm[INTK];
        CCPR2L = rekuRx.pwm[EXHT];
    }
}

void updateControlPVsFailsafe()
{
    CCPR1L = 64;
    CCPR2L = 64;
    letny_bypass(0);
}

#define MS2TICK(ms) ((ms)*5)
uint8_t timeout16(uint16_t t0, uint16_t delay)
{
    uint32_t t2 = TMR0;
    if(t0 > t2)
        t2 += 0x10000;
        
    return (t2 - t0) > delay;
}

#define US2TICK(us) ((us)*4)
uint8_t timeout8(uint8_t t0, uint8_t delay)
{
    uint16_t t2 = TMR2;
    if(t0 > t2)
        t2 += 256;
    return (t2 - t0) > delay;
}

uint8_t checkRxErr()
{
    if(RCSTA1 & 6){
        RCSTA1bits.CREN = 0;
        return 1;
    }
    return 0;
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

void letny_bypass(uint8_t on)
{
    if(on){
        PORTCbits.RC0 = 1;
    }
    else{
        PORTCbits.RC0 = 0;
    }
}

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
                if(TX1IF){
                    uint8_t* data = (uint8_t*)&rekuTx;
                    TXREG1 = data[txCtx.byte_idx++];
                }
            }
            else {
                if (TXSTA1bits.TRMT) {
                    txCtx.state = COMMTX_IDLE;
                    CLRWDT();
                    led_toggle();
                }
            }
        }
          break;
        default:
          break;
    }
}

#define SIZE_OF_ARRAY(array) (sizeof(array) / sizeof(array[0]))
void start_tx(uint8_t mark)
{
    if(txCtx.state != COMMTX_IDLE){
        return;
    }

    rekuTx.stat = mark | commState;
    
    for(uint8_t i=0; i<SIZE_OF_ARRAY(rekuCh); i++)
        rekuTx.ch[i] = rekuCh[i];
    
    rekuTx.ch[INTK].temp |= (uint16_t)(reset_condition_flags & 0x1F) << 10;
    rekuTx.ch[EXHT].temp |= (uint16_t)(reset_condition_flags >> 6) << 10;
    
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
    enum RxCommState state;
    int byte_idx;
    uint16_t t0Err;
    uint16_t t0Comm;
}rxCtx;

void do_rx()
{
    if(RC1IF){
        uint8_t* data = (uint8_t*)&rekuRx;
           data[rxCtx.byte_idx++] = RCREG1;
           if(checkRxErr()){
               rxCtx.state = COMMRX_ERR;
               rxCtx.byte_idx = 0;
           }
           if(rxCtx.state == COMMRX_READING){
               if(rxCtx.byte_idx == 1){
                   if((rekuRx.ctrl & mark_mask) != markCmd){
                       rxCtx.state = COMMRX_ERR;
                       rxCtx.byte_idx = 0;
                   }
                   else{
                       led_set();
                   }
               }
               else if(rxCtx.byte_idx == sizeof(rekuRx)){
                   uint8_t crc = 0;
                   for(uint8_t i = 0; i<sizeof(rekuRx)-1; i++){
                       crc = calc_crc(data[i], crc);
                   }
                   if( crc == rekuRx.crc){
                       rxCtx.t0Comm = TMR0;
                       commState = COMMUNICATING;
                       updateControlPVs();
                       start_tx(markCnf);
                   }
                   else{
                       rxCtx.state = COMMRX_ERR;
                   }
                   rxCtx.byte_idx = 0;
               }
           }
           else{
               rxCtx.byte_idx = 0;
           }
           
           if(rxCtx.state == COMMRX_ERR || rxCtx.byte_idx==1){
               rxCtx.t0Err = TMR0;
           }
    }
    else{
        if(rxCtx.state == COMMRX_ERR){
            if(timeout16(rxCtx.t0Err, MS2TICK(50))){
                rxCtx.state = COMMRX_READING;
                RCSTA1bits.CREN = 1;
            }
        }
        else{
            if(rxCtx.byte_idx > 0){
                if(timeout16(rxCtx.t0Err, MS2TICK(50))){
                    rxCtx.state = COMMRX_ERR;
                    rxCtx.byte_idx = 0;
                    rxCtx.t0Err = TMR0;
                }
            }
            else{
                if(timeout16(rxCtx.t0Comm, MS2TICK(1500))){
                    if(commState != NO_COMM){
                        commState = COMM_LOST;
                        updateControlPVsFailsafe();
                    }
                    rxCtx.t0Comm = TMR0;
                    start_tx(markInd);
                }
            }
        }
    }
}

uint8_t changeCh(uint8_t curCh)
{
    return curCh == INTK ? EXHT : INTK;
}

enum AdcState {SAMPLING, CONVERTING, BEFORE_NEXT};
const uint8_t chADC[2] = {0,1};
#define MEDBUFSIZE 5
#define AVGEXP 6
#define AVGCOUNT (1<<AVGEXP)
struct {
  enum RekuCh curCh;
  enum AdcState state;
  uint8_t t0;
  struct {
    uint16_t medbuf[MEDBUFSIZE];
    uint8_t medidx;
    uint16_t avgsum;
    uint8_t avgcnt;
  } ch[2];
} measCtx;

void swap(uint16_t *a, uint16_t *b){
    uint16_t tmp = *a;
    *a = *b;
    *b = tmp;
}

void bubbleSort(uint8_t chidx, uint8_t n)
{
    uint8_t i, j;
    uint8_t swapped;
    uint16_t* arr = measCtx.ch[chidx].medbuf;
    for (i = 0; i < n - 1; i++) {
        swapped = 0;
        for (j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                swap(&arr[j], &arr[j + 1]);
                swapped = 1;
            }
        }

        if (!swapped)
            break;
    }
}

uint16_t calc_med(uint8_t chidx)
{
    bubbleSort(chidx, MEDBUFSIZE);
    return measCtx.ch[chidx].medbuf[MEDBUFSIZE/2];
}

void do_meas_temp()
{
    if(measCtx.state == SAMPLING){
        if(timeout8(measCtx.t0, US2TICK(25))){
            ADCON0bits.GO_nDONE = 1;
            measCtx.state = CONVERTING;
        }
    }
    else if(measCtx.state == CONVERTING){
        if(!ADCON0bits.GO_nDONE){
            measCtx.t0 = TMR2;
            uint16_t val = ADRES;
            uint8_t *midx = &measCtx.ch[measCtx.curCh].medidx;
            if(*midx < MEDBUFSIZE){
                measCtx.ch[measCtx.curCh].medbuf[(*midx)++] = val;
                if(*midx == MEDBUFSIZE){
                    *midx = 0;
                    val = calc_med(measCtx.curCh);
                    if( measCtx.ch[measCtx.curCh].avgcnt++ < AVGCOUNT){
                        measCtx.ch[measCtx.curCh].avgsum += val;
                        if( measCtx.ch[measCtx.curCh].avgcnt == AVGCOUNT){
                            rekuCh[measCtx.curCh].temp = measCtx.ch[measCtx.curCh].avgsum >> AVGEXP;
                            measCtx.ch[measCtx.curCh].avgcnt = 0;
                            measCtx.ch[measCtx.curCh].avgsum = 0;
                            measCtx.curCh = changeCh(measCtx.curCh);
                        }
                    }
                }
            }
            measCtx.state = BEFORE_NEXT;
        }
    }
    else if(measCtx.state == BEFORE_NEXT){
        if(timeout8(measCtx.t0, US2TICK(4))){
            measCtx.state = SAMPLING;
            measCtx.t0 = TMR2;
            ADCON0bits.CHS = chADC[measCtx.curCh];
        }
    }
}

uint8_t tacho_bits;
const uint8_t tacho_masks[2] = {1<<5, 1<<4};
struct {
    uint8_t cnt;
    uint16_t t0;
} tacho[2];

void do_tacho()
{
    uint8_t diff = tacho_bits ^ PORTB;
    tacho_bits = PORTB;
    for(uint8_t ch=0; ch<2; ch++){
        if(diff & ~tacho_bits & tacho_masks[ch]){
             ++ tacho[ch].cnt;
            if(timeout16(tacho[ch].t0, MS2TICK(450))){
                rekuCh[ch].period = (TMR0 - tacho[ch].t0) / tacho[ch].cnt;
                tacho[ch].t0 = TMR0;
                tacho[ch].cnt = 0;
            }
        }
        else if(timeout16(tacho[ch].t0, MS2TICK(900))){
            tacho[ch].t0 = TMR0;
            tacho[ch].cnt = 0;
            rekuCh[ch].period = 0;
        }
    }
}

static void init()
{
    handle_reset_condition();

    TRISD7 = 0; // PCB orange LED
    TRISC0 = 0; // letny bypass
    letny_bypass(0);
    RBPU = 0;
    
    PR2 = 255;
    CCP1CON = (1<<4)|(1<<5); //duty bit0 bit1
    CCP2CON = (1<<4)|(1<<5);
    
    TRISC1 = 0; //CCP2
    TRISC2 = 0; //CCP1
    T2CONbits.T2CKPS = 1; // 3.2552us
    TMR2ON = 1;
    
    CCPR1L = 64;
    CCPR2L = 64;
    
    //set pwm mode
    CCP1CONbits.CCP1M |= 0xC;
    CCP2CONbits.CCP2M |= 0xC;
    
    //4.9152MHz
    T0CON = 0;
    TMR0ON = 1;
    T0CON |= 7; //1:256 T=3.653s dT=208us
    
    //ADCON1bits.VCFG = 0;
    ADCON1bits.PCFG = 0xD;
    measCtx.t0 = TMR2;
    ADCON0bits.CHS = chADC[measCtx.curCh];
    ADCON2bits.ADCS = 5; // fosc/16
    ADCON2bits.ADFM = 1;
    ADCON0bits.ADON = 1;
    measCtx.state = SAMPLING;
    
    init_usart();
}

void main(void)
{
    init();
    
    if(is_power_on_reset())
        start_tx(markInd);
    
    while(1) 
    {
        do_meas_temp();
        do_tacho();
        do_tx();
        do_rx();
    }
}

const uint8_t one_wire_crc8_table[] = {
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
B15 - J5.1 temp.sens. -- AN1 exhaust
B14 - J5.2 temp.sens. --- AVDD
B17 - J4.1 temp.sens. -- AN0 intake
B16 - J4.2 temp.sens. --- AVDD
A7 - 2k2 vdd pullup
B8 - J7.1
B7 - J7.2 -- RC1(CCP2)
B6 - J7.3
B5 - J7.4 -- RB4
B12 - J6.1
B11 - J6.2 -- RC2(CCP1)
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
