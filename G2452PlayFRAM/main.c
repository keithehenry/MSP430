#define SizeOfAudio 0x4DA6
// 19878

/*
The above audio is that of an 8kHz sample of a "chirp" sound, from the chirp_8kHz.wav file.

An 8-bit DAC (8-bit samples, using PWM, and a timer count of 256 per sample), requires a 
minimum timer clock rate of:
 f = 8kHz * 256 = 2MHz

However, this indirectly generates an 8kHz artifact in the PWM audio result. An
audio filter set to play speech, up to 4kHz, doesn't reduce the 8kHz artifact enough.

To overcome this, this code uses a timer clock of 8Mhz, and interpolates between every fourth
sample. The interpolation basically reduces the wavefile storage requirement by a factor of 4.

This version of the code doesn't use interrupts, for debuging the interpolation.
*/

#include <msp430.h>
// Debug
#define LedRED  BIT0                        // P1.1 is Red LED

// Used for ISRs:


//================ FM25V40 ==================
// The "FM25V40 BoosterPack for TI LaunchPad" is wired as follows:
// Port1
#define     SCK     BIT5                // P1.5 is SCK
#define     MISO    BIT6                // P1.6 is MISO (also GRN LED)
#define     MOSI    BIT7                // P1.7 is MOSI
// Port2
#define     CS_     BIT0                // P2.0 is CS_
#define     WP_     BIT1                // P2.1 is WP_
#define     HOLD_   BIT2                // P2.2 is HOLD_
// WP_ and HOLD_ are jumpered to a pullup to VCC

// Host should theoretically:  Wait 1ms; assert CS_; wait 450us; perform dummy-read.
// The dummy read appears necessary - to wake from FM25V40's Sleep Mode presumably.
void FM25V40_Init (void)
{
  // P1.x:
  P1SEL |= SCK | MISO | MOSI;           // Let USIP.x control these, including direction, etc.
  // P2.x:
  P2OUT |= CS_;                         // Turn off CS_
  P2DIR |= CS_;                         // And drive it

  /* SPI mode 0  {USICKPH, USICKPL} <= 0b10, SCK inactive LO, capture on next edge, change on next edge */
//  USICTL1 &= ~USII2C;                 // (default) Select SPI mode
//  USICKCTL &= ~USICKPL;               // (default) SCK inactive LO
  USICTL1 |= USICKPH;                   // Capture on 1st edge, change on following
  /* SPI mode 3 would be {USICKPH, USICKPL} <= 0b01, as F-RAM always captures on rising edge */
  
  // P1 bits 7, 6, and 5 enable; Master; and (default) hold in reset
  USICTL0 |= USIPE7 + USIPE6 + USIPE5 + USIMST + USISWRST; // Port, SPI master

  USICKCTL |= USIDIV_0 | USISSEL_2;     // Div by 1 | SMCLK.
  
  USICTL0 &= ~USISWRST;                 // USI released for operation
}

// Stop. (Deassert CS_)
void FM25V40_Stop (void)
{
  while (!(USICTL1 & USIIFG)) {}        // Wait for idle
  P2OUT |= CS_;                         // Deassert CS_
}

// Receive. (Tri-state TDO and shift in 8 bits.)
char FM25V40_Read (void)
{
  while (!(USICTL1 & USIIFG)) {}        // Wait for idle
  P2OUT &= ~CS_;                        // Assert CS_ (only for initial dummy-read)
  USICTL0 &= ~USIOE;                    // SDO disable
  USICNT = 8;                           // Read
  while (!(USICTL1 & USIIFG)) {}        // Wait
  return (USISRL);                      // Return 8-bits
}

// Send. (Assert CS_, drive TDO, and shift out 8 bits.)
void FM25V40_Wrte (char SndData)
{
  while (!(USICTL1 & USIIFG)) {}        // Wait for idle
  P2OUT &= ~CS_;                        // Assert CS_
  USICTL0 |= USIOE;                     // SDO enable
  USISRL = SndData;                     // write data
  USICNT = 8;                           // Send it
}

// Set Address.
void FM25V40_Addr (void)
{
  FM25V40_Wrte (0x03);
  FM25V40_Wrte (0x00);
  FM25V40_Wrte (0x00);
  FM25V40_Wrte (0x00);
}

//================ Timer DAC ==================
inline void TDAC_Init(void) {
  // Select TA0.2 to P1.4
  P1DIR |= BIT4;
  P1SEL |= BIT4;
  P1SEL2|= BIT4;

  // TAR to count from 0 to 255, interrupt, and reload automatically
  TA0CTL = TASSEL_2 + MC_1;                      // SMCLK, UP to CCR0 w/ auto reset
  TA0CCR0 = 255;                                 // Set PWM period to 256 clock ticks
  TA0CCTL0 = CCIE;                               // CCR0 interrupt enabled

  // TA0.2 (aka P1.4) is HI until CCR2 is reached, then goes LO.
  // CCR0's interrupt is used to reload CCR2.
  TA0CCR2 = 128;                                 // Start audio at midpoint
  TA0CCTL2 = OUTMOD_7;                           // Set at CCR0, reset at CCR2
}

inline void TDAC_Play(unsigned long AudioSize) {

  unsigned long i;
  unsigned int uAudSam1;
  unsigned int uAudSam2;
  unsigned int uAudX4;
  signed int sAudDiff;
  unsigned int uNxSam0, uNxSam1, uNxSam2, uNxSam3;
  

  uAudSam2 = FM25V40_Read();                      // Read initial audio sample as next
  uAudX4   = (uAudSam2 << 2) + 2;                 // Compute first sample times four, plus rounding bit
  
  for (i = AudioSize-1; i != 0; i--) {            // [Need a least two sample for interpolation]
    uAudSam1 = uAudSam2;                          // Save previous next into current
    uAudSam2 = FM25V40_Read();                    // Read new next sample
    sAudDiff = uAudSam2 - uAudSam1;               // Compute signed difference

    uNxSam0 = uAudSam1;                           // Sample 0. Same as uAudX4 >> 2.
    uAudX4 += sAudDiff;                           // Add 1/4 the difference
    uNxSam1 = uAudX4 >> 2;                        // Sample 1
    uAudX4 += sAudDiff;                           // Again
    uNxSam2 = uAudX4 >> 2;                        // Sample 2
    uAudX4 += sAudDiff;                           // Again
    uNxSam3 = uAudX4 >> 2;                        // Sample 3
    uAudX4 += sAudDiff;                           // Add 1/4 the difference
    // 0
    TA0CCTL0 &= ~CCIFG;                           // Wait for end of current duty cycle
    while (!(TA0CCTL0 & CCIFG));                  //  no GIE style
    TA0CCR2 = uNxSam0;                            // Load PWM interpolated sample
    // 1
    TA0CCTL0 &= ~CCIFG;                           // Wait for end of current duty cycle
    while (!(TA0CCTL0 & CCIFG));                  //  no GIE style
    TA0CCR2 = uNxSam1;                            // Load PWM interpolated sample
    // 2
    TA0CCTL0 &= ~CCIFG;                           // Wait for end of current duty cycle
    while (!(TA0CCTL0 & CCIFG));                  //  no GIE style
    TA0CCR2 = uNxSam2;                            // Load PWM interpolated sample
    // 3
    TA0CCTL0 &= ~CCIFG;                           // Wait for end of current duty cycle
    while (!(TA0CCTL0 & CCIFG));                  //  no GIE style
    TA0CCR2 = uNxSam3;                            // Load PWM interpolated sample
  }
  // Final
  TA0CCTL0 &= ~CCIFG;                             // Wait for end of current duty cycle
  while (!(TA0CCTL0 & CCIFG));                    //  no GIE style
  TA0CCR2 = uAudSam2;                             // Load final sample. Same as uAudX4 >> 2.

  FM25V40_Stop ();
}


//================ MAIN ==================
void main(void) {
  // Watchdog timer
  WDTCTL = WDTPW + WDTHOLD;

  // DCO = 16MHz, MCLK = DCO; SMCLK = DCO/1 (8MHz clock)
  DCOCTL = CALDCO_16MHZ;
  BCSCTL1 = CALBC1_16MHZ;
  BCSCTL2 = DIVS_1;
    
  TDAC_Init();                                   // Init Timer-DAC

  FM25V40_Init();                                // Init FM25V40
  FM25V40_Read();                                // Dummy-Read
  
  
  for (;;) {
    FM25V40_Addr();
    TDAC_Play(SizeOfAudio);
    __delay_cycles(6000000);
  }
}

/**
 * TimerA0 interrupt service routine

#if defined(__TI_COMPILER_VERSION__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
#else
  void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) TIMER0_A0_ISR (void)
#endif
{
  TA0CCR2 = uAudX4 >> 2;                          // Next interpolated sample
  __bic_SR_register_on_exit(LPM4_bits);           // Clear all LPM bits from SR on stack
  }  

}
// Compute samples first. Samples stay in registers. Saves a few program words.
main:
    0efae: 0a 12                     PUSH    R10
    0efb0: 09 12                     PUSH    R9
    0efb2: 08 12                     PUSH    R8
    0efb4: 07 12                     PUSH    R7
    0efb6: 06 12                     PUSH    R6
    0efb8: 05 12                     PUSH    R5
    0efba: 04 12                     PUSH    R4
    0efbc: b2 40 80 5a 20 01         MOV     #0x5a80, &0x0120
    0efc2: d2 42 f8 10 56 00         MOV.B   &0x10f8, &0x0056
    0efc8: d2 42 f9 10 57 00         MOV.B   &0x10f9, &0x0057
    0efce: e2 43 58 00               MOV.B   #0x0002, &0x0058
    0efd2: f2 d0 10 00 22 00         BIS.B   #0x0010, &0x0022
    0efd8: f2 d0 10 00 26 00         BIS.B   #0x0010, &0x0026
    0efde: f2 d0 10 00 41 00         BIS.B   #0x0010, &0x0041
    0efe4: b2 40 10 02 60 01         MOV     #0x0210, &0x0160
    0efea: b2 40 ff 00 72 01         MOV     #0x00ff, &0x0172
    0eff0: b2 40 10 00 62 01         MOV     #0x0010, &0x0162
    0eff6: b2 40 80 00 76 01         MOV     #0x0080, &0x0176
    0effc: b2 40 e0 00 66 01         MOV     #0x00e0, &0x0166
    0f002: 3a 40 02 02               MOV     #0x0202, R10
    0f006: 76 40 80 00               MOV.B   #0x0080, R6
    0f00a: 37 40 01 e0               MOV     #0xe001, R7
    0f00e: 74 47                     MOV.B   @R7+,   R4
    0f010: 09 44                     MOV     R4,     R9
    0f012: 09 86                     SUB     R6,     R9
    0f014: 0a 59                     ADD     R9,     R10
    0f016: 0c 4a                     MOV     R10,    R12
    0f018: b0 12 d4 f0               CALL    #0xf0d4
    0f01c: 08 4c                     MOV     R12,    R8
    0f01e: 0a 59                     ADD     R9,     R10
    0f020: 0c 4a                     MOV     R10,    R12
    0f022: b0 12 d4 f0               CALL    #0xf0d4
    0f026: 05 4c                     MOV     R12,    R5
    0f028: 0a 59                     ADD     R9,     R10
    0f02a: 0c 4a                     MOV     R10,    R12
    0f02c: b0 12 d4 f0               CALL    #0xf0d4
    0f030: 0a 59                     ADD     R9,     R10
    0f032: 92 c3 62 01               BIC     #0x0001, &0x0162
    0f036: 92 b3 62 01               BIT     #0x0001, &0x0162
    0f03a: fd 27                     JZ      0xf036
    0f03c: 82 46 76 01               MOV     R6,     &0x0176
    0f040: 92 c3 62 01               BIC     #0x0001, &0x0162
    0f044: 92 b3 62 01               BIT     #0x0001, &0x0162
    0f048: fd 27                     JZ      0xf044
    0f04a: 82 48 76 01               MOV     R8,     &0x0176
    0f04e: 92 c3 62 01               BIC     #0x0001, &0x0162
    0f052: 92 b3 62 01               BIT     #0x0001, &0x0162
    0f056: fd 27                     JZ      0xf052
    0f058: 82 45 76 01               MOV     R5,     &0x0176
    0f05c: 92 c3 62 01               BIC     #0x0001, &0x0162
    0f060: 92 b3 62 01               BIT     #0x0001, &0x0162
    0f064: fd 27                     JZ      0xf060
    0f066: 82 4c 76 01               MOV     R12,    &0x0176
    0f06a: 06 44                     MOV     R4,     R6
    0f06c: 37 90 a0 ef               CMP     #__FRAME_END__, R7
    0f070: ce 23                     JNZ     0xf00e
    0f072: 92 c3 62 01               BIC     #0x0001, &0x0162
    0f076: 92 b3 62 01               BIT     #0x0001, &0x0162
    0f07a: fd 27                     JZ      0xf076
    0f07c: 82 44 76 01               MOV     R4,     &0x0176
    0f080: 0d 12                     PUSH    R13
    0f082: 0e 12                     PUSH    R14
    0f084: 3d 40 5c 23               MOV     #0x235c, R13
    0f088: 3e 40 16 00               MOV     #0x0016, R14
    0f08c: 1d 83                     DEC     R13
    0f08e: 0e 73                     SBC     R14
    0f090: fd 23                     JNZ     0xf08c
    0f092: 0d 93                     TST     R13
    0f094: fb 23                     JNZ     0xf08c
    0f096: 3e 41                     POP     R14
    0f098: 3d 41                     POP     R13
    0f09a: 00 3c                     JMP     0xf09c
    0f09c: 30 40 02 f0               BR      #0xf002
 **/
