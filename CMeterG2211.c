/* CMeterG2211: The MSP430G2211 includes a comparator, which can be used to
 * determine capacitance in an RC circuit when coupled with TimerA.  The
 * comparator connects to the 1/4 Vcc reference and stops the timer when the
 * threshold is reached.  The time value determines the capacitance to an
 * accuracy determined primarily by the accuracy to which the resistance is
 * known.  Requires a calibrated clock for accurate timing.
 * 
 * This version does not include serial communication to a computer for reading
 * out the value, and so uses the LEDs on P1.0 and P1.6 to signal when a reading 
 * has been completed.  The user can then pause the programming in the CCS 
 * debugger and determine the value from the registers.  Resitor is connected
 * between P1.5 and P1.1; Capacitor is connected between P1.1 and ground.
 * 
 * Note that the jumper for the TXD on the LaunchPad must be removed to use P1.1
 * in this project.
 */
 
#include <msp430g2211.h>
 
#define LED1    BIT0
#define LED2    BIT6
#define BTN1    BIT3
#define VCTL    BIT5
#define AIN1	BIT1
 
/*  Global Variables  */
unsigned int overflows;
 
/*  Function Definitions  */
void P1init(void);
void CAinit(void);
void TAinit(void);
 
void main(void) {

    WDTCTL = WDTPW + WDTHOLD;   // disable watchdog

    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;       // Calibrated 1 MHz clock

    for(;;) {           // trap
        P1init();
        CAinit();
        TAinit(); 
        _BIS_SR(LPM0_bits + GIE);
        
        overflows = 0;      // reset overflow counter
        P1OUT = LED1;       // clear LED2, set LED1 (red) to indicate measuring
        TACTL |= MC_2;      // start timer, sets TA0.0 at overflow.
        while (overflows < 10);
    
        CACTL1 |= CAON;     // Turn on comparator.    
        TACCTL0 = OUTMOD_5 + CCIE; // start discharge on next overflow.
        TACCTL1 |= CM_2;    // start TA1 capture on falling edge.
        overflows = -1;     // reset overflow counter (accounting for overflow
                            // to start discharge).
        
        _BIS_SR(LPM0_bits + GIE);

    } 	
    
} // main
 
void P1init(void) {
    P1OUT = LED2;    // default LED2 (green) on to indicate ready.
    P1DIR = LED1 + LED2 + VCTL; // output on P1.0 and P1.6 for LEDs, P1.5 for 
                                // voltage control on the RC circuit.
    P1SEL = VCTL;    // Set P1.5 to TA0.0 output, controls charge/discharge

    P1IES = BTN1;   // falling edge for pulled-up button
    P1IFG &= ~BTN1; // clear interrupt flag before enabling
    P1IE = BTN1;    // enable interrupt for BTN1
  
} // P1init
 
void CAinit(void) {
    CACTL1 = CARSEL + CAREF_1;   // 0.25 Vcc ref on - pin.
    CACTL2 = P2CA4 + CAF;       // Input CA1 on + pin, filter output.
    CAPD = AIN1;                // disable digital I/O on P1.7 (technically
                                // this step is redundant)
} // CAinit
 
void TAinit(void) {
    TACTL = TASSEL_2 + ID_0 + MC_0;     // Use SMCLK (1 MHz Calibrated), no division,
                                        // stopped mode
    TACCTL0 = OUTMOD_1 + CCIE;          // TA0 sets VCTL at TACCR0
    TACCTL1 = CCIS_1 + SCS + CAP + CCIE;       // Use CAOUT for input, synchronize, set to 
                                        // capture mode, enable interrupt for TA1.
                                        // NOTE: Capturing mode not started.
} // TAinit

 
/*  Interrupt Service Routines  */
#pragma vector = PORT1_VECTOR
__interrupt void P1_ISR(void) {
    switch(P1IFG & BIT3) {
        case BIT3:
            P1IFG &= ~BIT3;  // clear the interrupt flag
            if ((P1OUT & LED1)==LED1)
                return; // taking sample, do nothing if button is pressed
            else {
                __low_power_mode_off_on_exit(); // button pressed; continue program
                return;
            }
        default:
            P1IFG = 0;  // clear any errant flags
            return;
    }
} // P1_ISR
    

#pragma vector = TIMERA0_VECTOR
__interrupt void TA0_ISR(void) {
    overflows++;    // TA0 interrupt means TA has seen 2^16 counts without CA trigger.
                    // rollover to 0 on TAR, so mark overflow.
} // TA0_ISR

#pragma vector = TIMERA1_VECTOR
__interrupt void TA1_ISR(void) {
    TACCTL1 &= ~(CM_2 + CCIFG); // Stop TA1 captures, clear interrupt flag.
    TACTL &= ~MC_2;     // turn off TA.
    P1OUT = LED2;       // Done measuring, switch to green led.
    __low_power_mode_off_on_exit();    // continue program
} // TA1_ISR
