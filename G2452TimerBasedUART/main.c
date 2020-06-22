//******************************************************************************
//  MSP430F20xx Demo - Timer_A, Ultra-Low Pwr UART 38400 Echo, 16MHz SMCLK
//
//  Description: Use Timer_A CCR0 & CCR1 hardware output modes and SCCI data
//  latch to implement full duplex UART function @ 19200 baud. Software does
//  not directly read and write to RX and TX pins, instead proper use of output
//  modes and SCCI data latch are demonstrated. Use of these hardware features
//  eliminates ISR latency effects as hardware insures that output and input
//  bit latching and timing are perfectly synchronised with Timer_A regardless
//  of other software activity. In the Mainloop the UART function readies the
//  UART to receive one character and waits in LPM0 with all activity interrupt
//  driven.
//  After a character has been received, the UART receive function forces exit
//  from LPM0 in the Mainloop which echo's back the received character.
//  MCLK = SMCLK = 16MHz calibrated value from Info Memory Segment A.
//  //* No external watch crystal is required *//	
//
//                   MSP430F20xx
//                -----------------
//            /|\|                 |
//             | |                 |
//             --|RST              |
//               |                 |
//               |     TA1/TXD/P1.2|-------->
//               |                 | 38400 8N1
//               |   CCI0A/RXD/P1.1|<--------
//               |                 |
//
//
//  P. Forstner
//  Texas Instruments Inc.
//  January 2009
//  Built with IAR Embedded Workbench Version: v4.11
//  Built with CCE Version: 3.1 Build 3.2.3.6.4
//******************************************************************************

// KEH 2020-06-21:
// IMPORTANT:
//   For this code, LaunchPad jumpers are in HW-UART mode. Strange but true!
// Required:
//   Changed include file and vector syntax for MSP430G2452.
//   Declared all ISR variables as volatile.
//   Added missing P1DIR for Red LED (and altered code to include Grn LED).
// Additional cosmetic changes and these comments.
// My system and/or LaunchPad G2 (original) only worked up to 9600 baud. Not sure why.
// Revised RXUARTDataValid and overrun detection

#define RXD       BIT1                      // RXD on P1.1
#define TXD       BIT2                      // TXD on P1.2

//   Conditions for 4800 Baud SW UART, SMCLK = 16MHz
// #define Bitime50 1666                       // ~50% bit length
// #define Bitime80 2666                       // ~ 80% bit length
// #define Bitime99 3300                       // ~ 99% bit length
// #define Bitime   3333                       // 16MHz / 4800 Baud = 3333

//   Conditions for 9600 Baud SW UART, SMCLK = 16MHz
#define Bitime50 833                        // ~50% bit length
#define Bitime80 1333                       // ~ 80% bit length
#define Bitime99 1650                       // ~ 99% bit length
#define Bitime   1666                       // 16MHz / 9600 Baud = 1667

//   Conditions for 19200 Baud SW UART, SMCLK = 16MHz
// #define Bitime50 416                        // ~50% bit length
// #define Bitime80 666                        // ~ 80% bit length
// #define Bitime99 825                        // ~ 99% bit length
// #define Bitime   833                        // 16MHz / 19200 Baud = 833

//   Conditions for 38400 Baud SW UART, SMCLK = 16MHz
// #define Bitime50 208                        // ~ 50% bit length
// #define Bitime80 333                        // ~ 80% bit length
// #define Bitime99 412                        // ~ 99% bit length
// #define Bitime   416                        // 16MHz / 38400 Baud = 416

#define LedRED  BIT0                        // P1.1 is Red LED
#define LedGRN  BIT6                        // P1.6 is Grn LED

unsigned char TXData;
volatile unsigned int RXTempData;
volatile unsigned int TXTempData;
volatile unsigned char RXData;
volatile unsigned char RXBitCnt;
volatile unsigned char TXBitCnt;
volatile unsigned char RXUARTDataValid;

void TX_UART (void);
void RX_UART_Start (void);

#include  <msp430.h>

void main (void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer

//  Initialize Timer for Software UART
  TACCTL1 = OUT;                            // TXD Idle as '1'
  TACTL = TASSEL_2 + MC_2;                  // SMCLK, continuous mode
  P1SEL = TXD + RXD;                        // Configure I/Os for UART
  P1DIR = TXD;                              // Configure I/Os for UART
  RXUARTDataValid = 0;                      // No char yet received

  P1DIR |= LedGRN | LedRED;                 // Set LEDs to Outputs
  P1OUT &= ~(LedGRN | LedRED);              // And turn them off
  
  // Borrowed code. May not be necessary.
  __delay_cycles(40000);                    // Time for VCC to rise to 3.3V
  if ((CALDCO_16MHZ == 0xFF) || (CALBC1_16MHZ == 0xFF))
  {
    while(1)                                // Blink LED if calibration data
    {                                       // missing
      P1OUT ^= LedRED;                      // Toggle LED
      __delay_cycles(60000);
    }
  }
  DCOCTL = CALDCO_16MHZ;                    // DCO = 16MHz calibrated
  BCSCTL1 = CALBC1_16MHZ;                   // DCO = 16MHz calibrated

// Mainloop
  RX_UART_Start();                          // UART ready to RX one Byte
  while (1)
  {
    __bis_SR_register(LPM0_bits + GIE);     // LPM0: keep DCO running
    P1OUT ^= LedGRN;                        // Each byte received toggles Grn LED

    TXData = RXData;                        // Send RX data to TX
    RXUARTDataValid = 0;                    // RX Data has been read
    TX_UART();                              // TX Back RXed Byte Received
  }
}

// =============================================================================
// Function Transmits Character from TXData Buffer
void TX_UART (void)
{
  while ( TACCTL1 & CCIE );                 // Wait for previous TX completion
  TXBitCnt = 10;                            // Load Bit counter, 8data + ST/SP
  TXTempData = 0xFF00 + TXData;             // Add stop bit + idle bits to TXData
  TACCR1 = TAR +14;                         // Current state of TA counter
                                            // + 14 TA clock cycles till first bit
  TACCTL1 =  OUTMOD2 + OUTMOD0 + CCIE;      // TXD = '0' = start bit, enable INT
}


// =============================================================================
// Function Readies UART to Receive Character into RXTXData Buffer
void RX_UART_Start (void)
{
  RXBitCnt = 8;                             // Load Bit counter
  TACCTL0 = SCS + OUTMOD0 + CM1 + CAP + CCIE;// Sync, Neg Edge, Cap
}

// =============================================================================
// Timer A0 interrupt service routine - UART RX
#if defined(__TI_COMPILER_VERSION__)
  #pragma vector=TIMER0_A0_VECTOR
  __interrupt void Timer_A0_ISR (void)
#else
  void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) TIMER_A0_ISR (void)
#endif
{
  TACCR0 += Bitime;                         // Add Offset to CCR0

  if( TACCTL0 & CAP )                       // Capture mode = start bit edge
  {
      TACCTL0 &= ~CAP;                      // Switch from capture to compare mode
      TACCR0 += Bitime50;
  }
  else
  {
    RXTempData = RXTempData >> 1;
    if (TACCTL0 & SCCI)                     // Get bit waiting in receive latch
      RXTempData |= 0x80;
    RXBitCnt --;                            // All bits RXed?
    if ( RXBitCnt == 0)
    {
      RXData = (char) RXTempData;
      if (RXUARTDataValid)
	while (1)                           // Another byte received before the last one taken
	  P1OUT |= LedRED;                  // Signal ERROR

      RXUARTDataValid = 1;                  // RXData is valid
      RXBitCnt = 8;                         // Re-Load Bit counter for next RX char
      TACCTL0 = SCS + OUTMOD0 + CM1 + CAP + CCIE; // Sync, Neg Edge, Cap
                                            // wait for next falling RX edge
                                            // which is the next start bit
      __bic_SR_register_on_exit(LPM4_bits); // Clear all LPM bits from SR on stack
    }
  }
}

// =============================================================================
// Timer A1 interrupt service routine - UART TX
#if defined(__TI_COMPILER_VERSION__)
  #pragma vector=TIMER0_A1_VECTOR
  __interrupt void Timer_A1_ISR (void)
#else
  void __attribute__ ((interrupt(TIMER0_A1_VECTOR))) TIMER_A1_ISR (void)
#endif
{
  switch (__even_in_range(TAIV, 10))        // Use calculated jump table branching
  {
    case  2 : if ( TXBitCnt == 0)           // TACCR1 CCIFG - UART TXD
              {  TACCTL1 &= ~CCIE;          // All bits TXed, disable interrupt
//               last bit = stop-bit has been transmitted
              }
              else
              {  if (TXBitCnt ==1)
                    TACCR1 += Bitime80;     // Stop bit is a bit shorter to
                 else                       // get time for CPU processing
                    TACCR1 += Bitime99;     // Add Offset to CCR0
                                            // For full speed echo TX should
                                            // be a bit faster than RX to
                                            // avoid overruns
                 TACCTL1 |=  OUTMOD2;       // TX '0'
                 if (TXTempData & 0x01)
                    TACCTL1 &= ~ OUTMOD2;   // TX '1'
                 TXTempData = TXTempData >> 1;
                 TXBitCnt --;
              }
              break;
  }
}
