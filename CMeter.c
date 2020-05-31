/* CMeter: The MSP430G2452 includes a comparator, which can be used to
 * determine capacitance in an RC circuit when coupled with TimerA.  The
 * comparator connects to the 1/4 Vcc reference and stops the timer when the
 * threshold is reached.  The time value determines the capacitance to an
 * accuracy determined primarily by the accuracy to which the resistance is
 * known.  Requires a calibrated clock for accurate timing.
 * 
 * The user can set a breakpoint with the debugger and view the registers.
 *
 * The external circuit is:
 * P1.5 is connected to a resistor. The resistor and capacitor are connected
 * and also connected to P1.1. The other terminal of the capacitor is connected 
 * to ground.
 *
 * The equation for calculating C is:
 *   C = t / (R * ln(4));
 * An R of 47K is interesting as each overflow ("timerhi") represents:
 *   C = 2**16 us / (47000 Ohms * ln(4) = ~1 uF
 * At this rate though a 330uF capacitor takes 22s to measure.
 * 
 * NOTE that the jumper for the TXD on the LaunchPad MUST BE REMOVED to use P1.1
 * in this project.
 *
 * The MSP430G2452 has two I/O ports, one "Comparator_A+" (aka Comparator A) and 
 *  one "Timer_A" (aka Timer0_A3).
 * Port1 will be used for red and green LEDs, sensing the button, charging/discharging
 *  the RC network, sensing the voltage levels, and debugging the comparator.
 * Comparator_A+ is used for sensing analog inputs. It will be used to trigger 
 *  an event when the capacitor voltage reaches 1/4 Vcc during discharge.
 * Timer_A counts the duration of the discharge.
 
 Scheme:
 - Set charge/discharge pin to charge.
 - Wait for button press in LPM.
 - Set charge/discharge pin to discharge.
 - Set Timer to time and Comparator to compare.
 - Wait for results in LPM.
 - Use debugger to read {timerhi, TAR} and compute capacitance!
 - 
 Notes:
 There are lots of ways to do this. The many choices resulted in settling for
 my favorite: conserving code space and RAM. Letting the comparator interrupt
 (without involving the Timer capture feature) would have been a good choice 
 but then its ISR would have to either turn off the Timer or squirrel away the 
 timer values while interrupts were disabled. Letting the Timer ISR deal with the
 Timer interrupts seemed like the better choice. I chose to leave the comparator 
 running so I could view the output on P1.7, but one could also turn in off most
 of the time to save power.
 */
 
#include <msp430.h>
 
// Pre-defined Launchpad pins
#define LED1    BIT0    // RED LED out
#define LED2    BIT6    // GRN LED out
#define BTN1    BIT3    // Left-Button in
#define AIN1	BIT1    // For TimerA comparitor in. REMOVE TXD JUMPER on Launchpad.
// Free pins
#define VCTL    BIT5    // For Voltage Control out
#define CAO     BIT7    // Monitor Comparator
 
/*  Global Variables  */
unsigned int timerhi;
 
void main(void) {
    WDTCTL = WDTPW + WDTHOLD;   // disable watchdog

    /* Eliminate linker warnings for unused port. */
    P2DIR = 0xFF;
    P2OUT = 0x00;

    /* Set DCO to calibrated 1 MHz clock */
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;

    /* Port 1 Config */
    // P1.7 CAO, out, connected to CAOUT (Comparator_A+ output) for debug
    // P1.5 VCTL, out, RC voltage control
    // P1.3 BTN1, in, button. (OUT and REN together select pullup on input)
    // P1.3 also interrupts
    // P1.1 AIN1, in, Cap voltage - selected by Comparator module
    P1OUT = CAO | LED2 | VCTL | BTN1;   // Observe CAOUT, GRN ON, charge RC network, pull BTN1 HI
    P1DIR = ~(BTN1 | AIN1);             // All outputs except P1.3 and P1.1.
    P1SEL = CAO;                        // Selects CAOUT to P1.7
    P1SEL2= 0;                          //  cont.
    P1REN = BTN1;                       // BTN1 requires a pullup. R34 not populated.
    P1IES = BTN1;                       // Interrupt Edge Select =< high to low
                                        // NOTE: P1IE not yet enabled
    // Comparator A+ Config
    // The comparator output is used to capture the timer and the timer interrupts
    CACTL1 = CARSEL | CAREF_1 | CAON;   // - pin, 0.25 Vcc, ON, (no interrupt)
    CACTL2 = P2CA4 | CAF;               // Input CA1 on + pin, filter output.
    
    for(;;) {
        /* Arm Button interrupt and wait */
        P1IFG = 0;                  // Clear Interrupt FlaGs before enabling
        P1IE = BTN1;                // Interrupt Enable for BTN1 only
        _BIS_SR(LPM0_bits + GIE);   // Wait for Button interrupt
        
        /* Start discharge, clear timer, and wait */
        P1OUT ^= (LED2 | LED1);     // GRN OFF; RED toggle - measuring
        timerhi = 0;                // Clear hi-order timer register
        
        // Start RC network discharge, ie GO!!!!
        P1OUT &= ~VCTL;
        
        // Timer A:
        // Use SMCLK, no division, continuous mode, clear TAR, interrupt on overflow
        TACTL = TASSEL_2 | ID_0 | MC_2 | TACLR | TAIE;
        // Timer Capture/Compare Reg 1:
        // Falling edge, CAOUT for input, synchronize, capture, enable interrupt
        TACCTL1 = CM_2 | CCIS_1 | SCS | CAP + CCIE;
        _BIS_SR(LPM0_bits);         // Wait for Comparator interrupt
        
        P1OUT |= VCTL | LED2;       // Charge again, signal GRN waiting for button
        /* Record values - set break here */
        // {timerhi, TAR} contains 32-bit count
        __no_operation();
        __no_operation();
    }
} // main
 
/*  Interrupt Service Routines  */
// Port1 interrupt is enabled for button push only
#pragma vector = PORT1_VECTOR
__interrupt void P1_ISR(void) {
    P1IE = 0;                       // Inhibit future P1 interrupts (until next time)
    __low_power_mode_off_on_exit(); // Button pressed; continue main program
} // P1_ISR
    

// Timer_A can interrupt for capture, triggered by the comparator, 
//  or if it overflows. Captures disables future interrupts and exits LPM,
//  whereas Timer overflows just keep counting.
#pragma vector = TIMER0_A1_VECTOR
__interrupt void TA0_ISR(void) {        // Reading TAIV clears highest priority int
    if (TAIV == 2) {                    // If Capture:
        TACTL = 0;                      //  Turn off Timer A interrupt
        TACCTL1 = 0;                    //  Turn off Capture interrupt
        __low_power_mode_off_on_exit(); //  Continue main program
    } else {                            // Timer A overflow
        timerhi++;                      //  Record overflow and keep counting
    }
} // TA0_ISR
