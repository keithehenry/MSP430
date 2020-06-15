//******************************************************************************
//  8-Bit PWM DAC
//
//  Description:
//  CCR0 <= 256; PWM period; bit resolution of the DAC.
//  CCR2 <= current sine sample, updated each time CCR0 interrupts.
//
//******************************************************************************
#include <msp430.h>

int counter;

const char sine[32] = {
  /*
  // sin(w)*256*(7/16)+128+15 => Range of 31 to 255.
  // Min of 31 allows enough time for the ISR SMCLK = 8MHz.
  // Filtered signal may be AC coupled anyway.
  143, 165, 186, 205, 222, 236, 246, 253,
  255, 253, 246, 236, 222, 205, 186, 165,
  143, 121, 100,  81,  64,  50,  40,  33,
   31,  33,  40,  50,  64,  81, 100, 121
  */
  /*
  // sin(w)*256*(1/4)+128
  128, 140, 152, 164, 173, 181, 187, 191,
  192, 191, 187, 181, 173, 164, 152, 140,
  128, 116, 104,  92,  83,  75,  69,  65,
   64,  65,  69,  75,  83,  92, 104, 116
  */
  // sin(w)*256*(7/16)+128
  // Works at MCLK = 16MHz, SMCLK = 8MHz - Glitches at MCLK = 8MHz, SMCLK = 8MHz
  // 16 cycles is enough / not enough time for the ISR.
  128, 150, 171, 190, 207, 221, 231, 238, 
  240, 238, 231, 221, 207, 190, 171, 150, 
  128, 106,  85,  66,  49,  35,  25,  18, 
   16,  18,  25,  35,  49,  66,  85, 106
  /*
  // sin(w)*256*(15/32)+128 - Glitches at 16MHz
  128, 151, 174, 195, 213, 228, 239, 246,
  248, 246, 239, 228, 213, 195, 174, 151,
  128, 105,  82,  61,  43,  28,  17,  10,
    8,  10,  17,  28,  43,  61,  82, 105
  */
  /*
  // sin(w)*256*(15/32)+128+7 - Runs at 16MHz MCLK, 8MHz SMCLK - looks a little flat on top
  135, 158, 181, 202, 220, 235, 246, 253,
  255, 253, 246, 235, 220, 202, 181, 158,
  135, 112,  89,  68,  50,  35,  24,  17,
   15,  17,  24,  35,  50,  68,  89, 112
  */
};

void main(void)
{
    // Watchdog timer
    WDTCTL = WDTPW + WDTHOLD;

    // DCO = 16MHz, MCLK = DCO; SMCLK = DCO/2;
    DCOCTL = CALDCO_16MHZ;
    BCSCTL1 = CALBC1_16MHZ;
    BCSCTL2= DIVS_1;
    
    // Select TA0.2 to P1.4
    P1DIR |= BIT4;
    P1SEL |= BIT4;
    P1SEL2|= BIT4;

    // Initialize Timer
    TA0CCTL0 = CCIE;                               // CCR0 interrupt enabled
    TA0CCTL1 = OUTMOD_7;                           // CCR1 Output Mode reset/set
    TA0CCTL2 = OUTMOD_7;                           // CCR2 Output Mode reset/set
    TA0CCR0 = 256;                                 // Set PWM period to 256 clock ticks
    TA0CCR2 = sine[0];                             // Load 1st sine sample
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
    counter = (counter + 1) & 0x1F;
    TA0CCR2 = sine[counter];                 // Load value for next duty cycle
}

