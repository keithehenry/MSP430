/*
 * main.c
 *
*/

/******************************************************************************
 * KEH 2020-05-26:
 *
 * Sets up ACLK and enters LPM3.
 * ACLK is routed to P1.0 (LED1) for observation.
 * ACLK interrupts and toggles GRN LED.
 * Waits forever in LPM.
 * 
 ******************************************************************************/
  
#include  "msp430.h"

#define     LED1                  BIT0
#define     LED2                  BIT6

void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

  /* Low-power warnings */
  P2DIR = 0xFF;
  P2OUT = 0x00;
  
  /* No P3 on MSP430G4253
  P3DIR = 0xFF;
  P3OUT = 0x00;
  */

  // setup port for LEDs
  P1DIR = 0xFF;                     // Set to all outputs to conserves power
  P1OUT = LED1;                     // Select ACLK
  P1SEL = LED1;                     //  ditto.
    
  /* these next two lines configure the ACLK signal to come from 
     a secondary oscillator source, called VLO.
     VLO is typically 12kHz, 4 min, 20 max.
    */
  BCSCTL1 |= DIVA_3;             // ACLK is 1 / 2**3 the speed of the source (VLO)
  BCSCTL3 |= LFXT1S_2;           // ACLK = VLO
  
  /* ((1 / 12000) * 8) * 1500 = 1 sec */
  /* Set up a timer to fire an interrupt periodically. 
     When the timer hits its limit, the interrupt will toggle the lights.
     We're using ACLK as the timer source, since it lets us go into LPM3
     (where SMCLK and MCLK are turned off). */
  TACCR0 = 1500;                 //  period
  TACTL = TASSEL_1 | MC_1;       // TACLK = ACLK, Up mode.  
  TACCTL1 = CCIE + OUTMOD_3;     // TACCTL1 Capture Compare
  TACCR1 = 750;                  // duty cycle
  __bis_SR_register(LPM3_bits + GIE);   // LPM3 with interrupts enabled
  // in LPM3, MCLCK and SMCLK are off, but ACLK is on.
  
  /* Never gets here */
}

// this gets used in pre-application mode only to toggle the lights:
#if defined(__TI_COMPILER_VERSION__)
#pragma vector=TIMER0_A1_VECTOR
__interrupt void ta1_isr (void)
#else
  void __attribute__ ((interrupt(TIMER0_A1_VECTOR))) ta1_isr (void)
#endif
{
  TACCTL1 &= ~CCIFG;        // reset the interrupt flag
  P1OUT ^= LED2;            // toggle GRN LED
}
