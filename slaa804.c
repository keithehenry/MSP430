//******************************************************************************
//  8-Bit PWM DAC
//
//  Description:
//  CCR0 <= 256; PWM period; bit resolution of the DAC.
//  CCR2 <= current sine sample, updated each time CCR0 interrupts.
//
//******************************************************************************
#include <msp430.h>

unsigned char counter;

const unsigned char sine[32] = {
  /*
  // 1/4
    128, 140, 152, 164, 173, 181, 187, 191,
    192, 191, 187, 181, 173, 164, 152, 140,
    128, 116, 104,  92,  83,  75,  69,  65,
     64,  65,  69,  75,  83,  92, 104, 116
  */
  // 7/16 - Works at MCLK = 16MHz, SMCLK = 8MHz - Glitches at MCLK = 8MHz, SMCLK = 8MHz
    128, 150, 171, 190, 207, 221, 231, 238, 
    240, 238, 231, 221, 207, 190, 171, 150, 
    128, 106,  85,  66,  49,  35,  25,  18, 
     16,  18,  25,  35,  49,  66,  85, 106
  /*
  // 15/32
    128, 151, 174, 195, 213, 228, 239, 246,
    248, 246, 239, 228, 213, 195, 174, 151,
    128, 105,  82,  61,  43,  28,  17,  10,
      8,  10,  17,  28,  43,  61,  82, 105
  */
};

void main(void)
{
    // Watchdog timer and clock
    WDTCTL = WDTPW + WDTHOLD;

    // DCO = 16MHz, MCLK = DCO; SMCLK = DCO/2;
    DCOCTL = CALDCO_8MHZ;
    BCSCTL1 = CALBC1_8MHZ;
    BCSCTL2= DIVS_0;
    
    // Select TA0.2 to P1.4
    P1DIR |= BIT4;
    P1SEL |= BIT4;
    P1SEL2|= BIT4;

    // Initialize Timer
    TA0CCTL0 = CCIE;                               // CCR0 interrupt enabled
    TA0CCTL1 = OUTMOD_7;                           // CCR1 Output Mode reset/set
    TA0CCTL2 = OUTMOD_7;                           // CCR2 Output Mode reset/set
    TA0CCR0 = 256;                                 // Set PWM period to 256 clock ticks
    TA0CCR2 = sine[counter];                       // Load 1st sine sample
    TA0CTL = TASSEL_2 + MC_1 + TACLR;              // SMCLK, upmode, clear TA1R

    _BIS_SR(LPM0_bits + GIE);                      // Enter LPM0 w/ interrupt
}

/**
 * TimerA0 interrupt service routine
 **/
#if defined(__TI_COMPILER_VERSION__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
#else
  void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) TIMER0_A0_ISR (void)
#endif
{
    counter++;                                    // increment counter
    TA0CCR2 = sine[counter%32];                   // Load value for next duty cycle
}

