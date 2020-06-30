//******************************************************************************
//  8-Bit PWM DAC
//
//  Description:
//  CCR0 <= 256; PWM period; bit resolution of the DAC.
//  CCR2 <= current sine sample, updated each time CCR0 interrupts.
//
//******************************************************************************
#include <msp430.h>

const char Sine[32] = {
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
  /*
  // sin(w)*256*(7/16)+128
  // Works at MCLK = 16MHz, SMCLK = 8MHz - Glitches at MCLK = 8MHz, SMCLK = 8MHz
  // 16 cycles is enough / not enough time for the ISR.
  128, 150, 171, 190, 207, 221, 231, 238, 
  240, 238, 231, 221, 207, 190, 171, 150, 
  128, 106,  85,  66,  49,  35,  25,  18, 
   16,  18,  25,  35,  49,  66,  85, 106
  */
  /*
  // sin(w)*256*(15/32)+128 - With new improved ISR, ALMOST works at MCLK = 16MHz, SMCLK = 8MHz
  // Minimum value of 8 didn't work (via Osc.). Changing it to 9 did.
  128, 151, 174, 195, 213, 228, 239, 246,
  248, 246, 239, 228, 213, 195, 174, 151
  128, 105,  82,  61,  43,  28,  17,  10,
    8,  10,  17,  28,  43,  61,  82, 105,
  */
  // sin(w)*256*(15/32)+128 with (i+0.5)/Samples. Works at MCLK = 16MHz, SMCLK = 8MHz
  140, 163, 185, 204, 221, 234, 243, 247,
  247, 243, 234, 221, 204, 185, 163, 140,
  116,  93,  71,  52,  35,  22,  13,   9,
    9,  13,  22,  35,  52,  71,  93, 116
  /*
  // sin(w)*256*(15/32)+128+7 - Runs at 16MHz MCLK, 8MHz SMCLK - looks a little flat on top
  135, 158, 181, 202, 220, 235, 246, 253,
  255, 253, 246, 235, 220, 202, 181, 158,
  135, 112,  89,  68,  50,  35,  24,  17,
   15,  17,  24,  35,  50,  68,  89, 112
  */
};

// ISR:
volatile char * pSine;
volatile char NexSine;

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
    TA0CCTL2 = OUTMOD_7;                           // CCR2 Output Mode reset/set
    TA0CCR0 = 255;                                 // Set PWM period to 256 clock ticks
    TA0CCR2 = Sine[0];                             // Load 1st sine sample
    TA0CTL = TASSEL_2 + MC_1 + TACLR;              // SMCLK, upmode, clear TAR

    // Get set and GO
    NexSine = Sine[1];                             // Prime the 2nd sample
    pSine = (char *) &Sine + 2;                    // Get address of next element into pointer
    _BIS_SR(LPM0_bits + GIE);                      // Enter LPM0 w/ interrupt
}
/*
    0e07a: b2 40 80 00 76 01         MOV     #0x0080, &0x0176
    0e080: b2 40 14 02 60 01         MOV     #0x0214, &0x0160
    0e086: f2 40 96 ff 02 02         MOV.B   #0x0096, &NexSine
    0e08c: b2 40 02 e0 00 02         MOV     #0xe002, &pSine
    0e092: 32 d0 18 00               BIS     #0x0018, SR
    0e096: 30 41                     RET     
*/
/**
 *** TimerA0 ISR ***

 Load TA0CCR2 as quickly as possible, as it may be a very small value.
 Compute the next value over the course of a full PWM period.
 This could/should be done outside the ISR. Doesn't matter here though.
**/

#if defined(__TI_COMPILER_VERSION__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
#else
  void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) TIMER0_A0_ISR (void)
#endif
{
    TA0CCR2 = NexSine;                            // Load value for next duty cycle
    NexSine = *pSine;
    if (++pSine == ((char *) &Sine + 32)) {
      pSine = (char *) &Sine;                     // Reset it
    }
}
/*
TIMER0_A0_ISR:
    0e098: 0c 12                     PUSH    R12
    0e09a: 5c 42 02 02               MOV.B   &NexSine, R12
    0e09e: 82 4c 76 01               MOV     R12,    &0x0176
    0e0a2: 1c 42 00 02               MOV     &pSine, R12
    0e0a6: f2 4c 02 02               MOV.B   @R12+,  &NexSine
    0e0aa: 3c 90 20 e0               CMP     #__FRAME_END__, R12
    0e0ae: 04 24                     JZ      0xe0b8
    0e0b0: 82 4c 00 02               MOV     R12,    &pSine
    0e0b4: 3c 41                     POP     R12
    0e0b6: 00 13                     RETI    
    0e0b8: b2 40 00 e0 00 02         MOV     #Sine,  &pSine
    0e0be: 3c 41                     POP     R12
    0e0c0: 00 13                     RETI    
*/
