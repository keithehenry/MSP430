//******************************************************************************
//  FRAM loader
//
//  With the appropriate host software, this code can write and read to and 
//  from an FM25V40 BoosterPack FRAM. 
//
//  The FM25V40 BoosterPack has already been tested. (See G2452TestFM25V40.)
//  Don't forget to remove the GRN LED jumper and place it between TDI and TDO on the
//   BoosterPack.
//  The Timer-based UART has also been tested. (See G2452TimerBasedUART.)
//
//  This code implements the following scheme:
//  Interpret received bytes as follows:
//    0x06: Assert CS_ and drive the NEXT byte onto TDO
//    0x04: Stop driving TDO but shift ONE byte from TDI, send received byte to host.
//    0x02: Deassert CS_
//    Any other code signals error.
//  This host is responsible for arranging these basic opcodes into FRAM operations.
//
// IMPORTANT:
//   For this code, LaunchPad jumpers are in HW-UART mode. Strange but true!

#define RXD       BIT1                      // RXD on P1.1
#define TXD       BIT2                      // TXD on P1.2

//   Conditions for 9600 Baud SW UART, SMCLK = 16MHz
#define Bitime50 833                        // ~50% bit length
#define Bitime80 1333                       // ~ 80% bit length
#define Bitime99 1650                       // ~ 99% bit length
#define Bitime   1666                       // 16MHz / 9600 Baud = 1667

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

  // Borrowed code. Not sure about this.
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
  RX_UART_Start();                          // UART ready to RX one byte
  while (1)
  {
    __bis_SR_register(LPM0_bits + GIE);     // Wait for RX byte

    // A byte has been received...
    //    P1OUT ^= LedGRN;                        // Each byte received toggle Grn LED

    // Should be after unloading
    RXUARTDataValid = 0;                    // RX Data has been read

    if (RXData & 0xF9)                      // Valid command?
      while (1) P1OUT |= LedRED;            // NO. Signal Error
    else
      switch (__even_in_range(RXData, 6))   // Use calculated jump table branching
	{
	case  0 :
	  while (1) P1OUT |= LedRED;        // ERROR.
//    0x02: Deassert CS_
	case  2 :
	  P1OUT &= ~LedGRN;                 // Turn OFF Grn
	  P1OUT |=  LedRED;                 // Turn OFF Red
	  break;
//    0x04: Stop driving TDO but shift ONE byte from TDI, send received byte to host.
	case  4 :
	  P1OUT &= ~LedGRN;                 // Turn OFF Grn
	  P1OUT &= ~LedRED;                 // Turn OFF Red
	  TX_UART();                        // TX Back RXed Byte Received
	  break;
	case  6 :
//    0x06: Assert CS_ and drive the NEXT byte onto TDO
	  P1OUT |=  LedGRN;                  
	  P1OUT |=  LedRED;                 // Turn OFF Red
	  __bis_SR_register(LPM0_bits + GIE);     // Wait for next RX byte

	  P1OUT |=  LedGRN;                  
	  P1OUT &= ~LedRED;                 // Turn OFF Red

	  TXData = RXData;
	  RXUARTDataValid = 0;                  // RX Data has been read
	  break;
	}

  }
}

// =============================================================================
// Function Transmits Character from TXData Buffer
void TX_UART (void)
{
  while ( TACCTL1 & CCIE );                 // Wait for previous TX completion
                                            // Prevents overruns for TXData
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
//      last bit has been received
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
