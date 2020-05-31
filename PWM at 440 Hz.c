/*
 * MSP430 Timer Tutorial Example Code 2
 * Anthony Scranney
 * www.Coder-Tronics.com
 * August 2014
 *
 * KEH: Modified for one Timer (4253)
 * PWM example using TimerA_0
 */

#include <msp430.h>

int main(void) {

	/*** Watchdog timer and clock Set-Up ***/
	WDTCTL = WDTPW + WDTHOLD;		// Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;  		// Set range
	DCOCTL = CALDCO_8MHZ;   		// Set DCO step + modulation

	/*** GPIO Set-Up ***/
	P1DIR |= BIT2;		       		// P1.2 set as output
	P1SEL |= BIT2;	       			// P1.2 selected Timer0_A Out1
	
	P2DIR = 0xFF;
	P2OUT = 0XFF;

	/*** Timer0_A Set-Up ***/
	TA0CCR0 |= 18182 - 1;			// PWM Period
	TA0CCTL1 |= OUTMOD_7;			// TA0CCR1 output mode = reset/set
	TA0CCR1 |= 9091;       			// TA0CCR1 PWM duty cycle
	TA0CTL |= TASSEL_2 + MC_1;		// SMCLK, Up Mode (Counts to TA0CCR0)

	_BIS_SR(LPM0_bits);		        // Enter Low power mode 0
}
