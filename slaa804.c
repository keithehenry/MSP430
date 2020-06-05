/* --COPYRIGHT--,BSD
 * Copyright (c) 2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//*******************************************************************************
//  MSP430FR2000 Demo - 8-Bit PWM DAC
//
//  Description: This demonstrates implementing a PWM DAC with Timer0_B. CCR0 is
//  configured with it's interrupt enabled 256 as the PWM period, which is a
//  also the bit resolution of the DAC. CCR1 is configured with the current sine
//  sample and is updated each time CCR0's interrupt fires. CCR2 is configured
//  with a 100 and unchanged to generate a DC signal. Timer_B is sourced from
//  2MHz SMCLK, which is sourced from DCO which has been chosen as 16MHz for the
//  advantage of no additional locking logic is needed. The RC networks are
//  described in SLAAxxx.
//
//  SMCLK = MCLK/2 = 2MHz; MCLK = DCO/4 = 8MHz; DCO = 16 MHz.
//
//
//
//
//           MSP430FR2000
//         ---------------
//     /|\|               |
//      | |     P1.6/TB0.1|--[RC Network]--> 250Hz Sine
//      --|RST            |
//        |               |
//        |               |
//        |     P1.7/TB0.2|--[RC Network]--> DC
//
//
//   Cameron P. LaFollette
//   Texas Instruments Inc.
//   Sept. 2017
//   Built with IAR Embedded Workbench vX.xx & Code Composer Studio v7.2
//******************************************************************************
#include <msp430.h> 

unsigned char counter;                             // pointer to next sine value

const unsigned char sine[32] = {                   // sine PWM samples
    128, 140, 152, 164, 173, 181, 187, 191,
    192, 191, 187, 181, 173, 164, 152, 140,
    128, 116, 104, 92, 83, 75, 69, 65,
    64, 65, 69, 75, 83, 92, 104, 116 };

void main(void)
{
    WDTCTL = WDTPW + WDTHOLD;                      // Stop WDT

    P1DIR |= BIT6 | BIT7;                          // P1.6 and P1.7 output
    P1SEL1 |= BIT6 | BIT7;                         // P1.6 and P1.7 options select

    counter = 0;                                   // init sine sample pointer

    //Initialize Cloc System
    __bis_SR_register(SCG0);                       // disable FLL
    CSCTL3 |= SELREF__REFOCLK;                     // Set REFO as FLL reference source
    CSCTL0 = 0;                                    // clear DCO and MOD registers
    CSCTL1 &= ~(DCORSEL_7);                        // Clear DCO frequency select bits first
    CSCTL1 |= DCORSEL_5;                           // Set DCO = 16MHz
    CSCTL2 = FLLD_0 + 487;                         // DCOCLKDIV = 16MHz
    __delay_cycles(3);
    __bic_SR_register(SCG0);                       // enable FLL
    while(CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1));     // FLL locked

    CSCTL5 |= DIVM_2 | DIVS_1;                     // MCLK = DCO/4 = 4MHZ,
                                                   // SMCLK = MCLK/2 = 2MHz

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;



    // Initialize Timer
    TB0CCTL0 = CCIE;                               // CCR0 interrupt enabled
    TB0CCTL1 = OUTMOD_7;                           // CCR1 Output Mode reset/set
    TB0CCTL2 = OUTMOD_7;                           // CCR2 Output Mode reset/set
    TB0CCR0 = 256;                                 // Set PWM period to 256 clock ticks
    TB0CCR1 = sine[counter];                       // Load 1st sine sample
    TB0CCR2 = 257;                                 // Load DC sample
    TB0CTL = TBSSEL_2 + MC_1 + TBCLR;              // SMCLK, upmode, clear TA1R

    _BIS_SR(LPM0_bits + GIE);                      // Enter LPM0 w/ interrupt
}

/**
 * TimerB0 interrupt service routine
 **/
#pragma vector=TIMER0_B0_VECTOR
__interrupt void TIMER0_B0_ISR(void)
{

    TB0CCR1 = sine[counter%32];                   // Load value for next duty cycle
    counter++;                                    // increment counter

    /*
    comment above 2 lines and uncomment below for ramp signal on p1.6
    */

    //counter+=8;                                   //change step size to change frequency
    //TB0CCR1 = counter & 0x0FF;                    // and off unwanted bits for 256 count
}

