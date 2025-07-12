// PIC16F628A Configuration Bit Settings
// 'C' source line config statements
// CONFIG
#pragma config FOSC = INTOSCIO  // Oscillator Selection bits (INTOSC oscillator: I/O function on RA6/OSC2/CLKOUT pin, I/O function on RA7/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = ON       // Power-up Timer Enable bit (PWRT enabled)
#pragma config MCLRE = OFF      // RA5/MCLR/VPP Pin Function Select bit (RA5/MCLR/VPP pin function is digital input, MCLR internally tied to VDD)
#pragma config BOREN = ON       // Brown-out Detect Enable bit (BOD enabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable bit (RB4/PGM pin has digital I/O function, HV on MCLR must be used for programming)
#pragma config CPD = OFF        // Data EE Memory Code Protection bit (Data memory code protection off)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>

/*
 * File:   main.c
 * Author: Olivier
 *
 * Created on 20 décembre 2024, 22:53
 */

// Pins usage
//    1 - Input  - VREF - looped to comparator             -> Do not connect anything here
//   18 - Input  - AN1  - to comparator                    -> Duration, Variable resistor
//    6 - Output - RB0                                     -> PiPower
//    7 - Input  - RB1  - WeakPullUp                       -> PowerLoss
//    8 - Output - RB2  -                                  -> TMR0 overflow (toggled)
//   11 - Input  - RB5  - WeakPullUp - Interrupt on change -> PiRunning (inverted signal from pin)

// we use the internal oscillator in "slow" mode
#define _XTAL_FREQ 48000

#include <xc.h>
#include <pic16f628a.h>
#include <stdint.h>

#define PiPower PORTBbits.RB0
#define PowerLoss PORTBbits.RB1
#define PiRunning (!PORTBbits.RB5)
#define Tmr0Overflow PORTBbits.RB2

// Direct value as it controls a N channel mosfet
#define PowerOn 1
#define PowerOff 0 

// use a define to "inline" the function and avoid the 1510 advisory about using it both in main and the ISR
#define disableTimer0Interrupt() INTCONbits.T0IE = 0
#define enableTimer0Interrupt()  INTCONbits.T0IE = 1

uint8_t readExpectedDelay();

uint8_t currentDelay = 0;

void main(void) {
    // setup ports as early as possible
    TRISAbits.TRISA1 = 1;  // input 
    TRISAbits.TRISA2 = 1;  // input for VREF to be seen

    TRISBbits.TRISB0 = 0;  // output
    TRISBbits.TRISB1 = 1;  // input
    TRISBbits.TRISB2 = 0;  // output
    TRISBbits.TRISB5 = 1;  // input
    
    PORTB = 0x00; // everything low level by default
    
    // activate power to the PI
    PiPower = PowerOn;
    
    PCONbits.OSCF = 0; // 48kHz to get long delays from TMR0
    
    OPTION_REGbits.nRBPU = 0;  // weak pull ups on port B
    OPTION_REGbits.T0CS = 0;  // TMR0 on internal clock
    OPTION_REGbits.PSA = 0;  // Prescaler for TMR0
    OPTION_REGbits.PS = 0b101; // 1:64 prescaler 
    
    // Timer0 will overflow every (4 * 64 * 256) / 48000 = 1.37 seconds which gives us
    // a wait time with 16 levels from 1.37 to 16*1.37 = 21.84 seconds
    
    INTCONbits.GIE = 1; // enable interrupts
    disableTimer0Interrupt(); // Timer0 interrupt (disable by default)
    INTCONbits.RBIE = 0; // Interrupts on port B (disable by default)
    
    CMCONbits.CM = 0b101; // one comparator (C2) on RA1/RA2(VREF)
    CMCONbits.C2INV = 0;  // comparator C2 output is not inverted
    
    VRCONbits.VREN = 1; // VREF enabled
    VRCONbits.VROE = 1; // VREF output on RA2
    VRCONbits.VRR = 0; // High range
    
    Tmr0Overflow = 0;
    TMR0 = 0;

    // let everything settle
    __delay_ms(100);
    
    // enable portb interrupts now that setup is done
    INTCONbits.RBIE = 1;
    
    while (1)
    {
        
        int expectedDelay = readExpectedDelay();
        
        if (PowerLoss)
        {
            if (currentDelay > expectedDelay)
            {
                disableTimer0Interrupt();
                Tmr0Overflow = 0;
                currentDelay = 0;
                PiPower = PowerOff;
            }
        }
        else
        {
            disableTimer0Interrupt();
            Tmr0Overflow = 0;
            PiPower = PowerOn;
        }
    }
}

void StartTimer()
{
    TMR0 = 0;
    Tmr0Overflow = 0;
    currentDelay = 0;
    enableTimer0Interrupt();
}

void __interrupt() interruptServiceRoutine(void)
{
    if (INTCONbits.RBIF)
    {
        INTCONbits.RBIF = 0;  // must reset for next time
        
        if (INTCONbits.RBIE)  // the port B change flag may be set even if the interrupt is disabled
        {
            if (PowerLoss && !PiRunning)
            {
                StartTimer();
            }
        }
    }
    
    if (INTCONbits.T0IF)
    {
        INTCONbits.T0IF = 0;  // must reset for next time
        
        if (INTCONbits.T0IE)  // the timer overflow flag keeps getting set even if the interrupt is disabled
        {
            Tmr0Overflow = 1u - Tmr0Overflow;
            currentDelay++;
        }
    }
}

uint8_t readExpectedDelay()
{
    // As there is no ADC on the 16F628A, we use the comparator module to 
    // compare VREF to the analog voltage on RA1
    // Changing VREF repeatedly until the comparison tells us RA1 is above VREF
    // gives us 16 discrete levels from the analog voltage, aka "poor man's ADC".
    uint8_t result = 0;
    do 
    {
        VRCONbits.VR = result;
        __delay_us(10 + 5);  // 10 for VREF settling + 5 for comparator update
        result++;
    }
    while ((result <= 0x0F) && (!CMCONbits.C2OUT));
    
    return result;
}
