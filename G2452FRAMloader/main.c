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
//    0x06: (Wrte) Assert CS_ and drive the NEXT byte onto TDO
//    0x04: (Read) Stop driving TDO but shift ONE byte from TDI, send received byte to host.
//    0x02: (Stop) Deassert CS_
//    Any other code signals error.
//  This host is responsible for arranging these basic opcodes into FRAM operations.
//
// IMPORTANT:
//   For this code, LaunchPad jumpers are in HW-UART mode. Strange but true!

#include <msp430.h>
// Debug
#define LedRED  BIT0                        // P1.1 is Red LED

//================ Software UART ==================
#define RXD       BIT1                      // RXD on P1.1 [HW-UART JUMPER SETTINGS]
#define TXD       BIT2                      // TXD on P1.2 [HW-UART JUMPER SETTINGS]

//   Conditions for 9600 Baud SW UART, SMCLK = 16MHz
#define Bitime50 833                        // ~50% bit length
#define Bitime80 1333                       // ~ 80% bit length
#define Bitime99 1650                       // ~ 99% bit length
#define Bitime   1666                       // 16MHz / 9600 Baud = 1667

volatile unsigned char RXData;
volatile unsigned char RXTempData;
volatile unsigned char RXBitCnt;
volatile unsigned char RXUARTDataValid;
volatile unsigned char TXData;
volatile unsigned char TXBitCnt;

// BEWARE: Both TX and RX require interrupts
// Initializes UART. Sets to receive characters.
void UART_Init (void)
{
  // Initialize Timer
  TACTL = TASSEL_2 + MC_2;                  // SMCLK, continuous mode
  TACCTL1 = OUT;                            // OUTMOD = 0, TXD <= '1' for initial idle state only

  // Initialize Ports - after setting OUTMOD and OUT
  P1SEL |= TXD + RXD;                       // Select TimerA0
  P1DIR |= TXD;                             // TXD is Output

  // Prime to receive first byte
  RXUARTDataValid = 0;                      // No char yet received (overrun detection)
  RXBitCnt = 8;                             // Load Bit counter
  TACCTL0 = SCS + OUTMOD0 + CM1 + CAP + CCIE;// Sync, Neg Edge, Cap
}
// Transmit character from TXData Buffer
void TX_UART (void)
{
  while ( TACCTL1 & CCIE );                 // Wait for previous TX completion (no overrun possible)
  TXBitCnt = 10;                            // Load Bit counter, 8 data + Start + Stop
  TACCR1 = TAR +14;                         // Current state of TA counter
                                            // + 14 TA clock cycles till first bit (after next statment)
  TACCTL1 =  OUTMOD2 + OUTMOD0 + CCIE;      // Reset on Interrupt. E.I. TXD <= '0' (Start Bit)
}                                           // => CCIE doubles as 'active' flag


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

  USICKCTL |= USIDIV_7 | USISSEL_2;     // Div by 128 (FIX!) SMCLK.
  
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


//================ MAIN ==================
void main (void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer

  // Initialize Debug
  P1DIR |= LedRED;                          // Set LED to Output
  P1OUT &= ~LedRED;                         // And turn it off

  // Crank up DCO to 16MHz
  // Borrowed code. Not sure about this.
  __delay_cycles(40000);                    // Time for VCC to rise to 3.3V
  if ((CALDCO_16MHZ == 0xFF) || (CALBC1_16MHZ == 0xFF))
  {
    for (;;)                                // Blink LED if calibration data
    {                                       // missing
      P1OUT ^= LedRED;                      // Toggle LED
      __delay_cycles(60000);
    }
  }
  DCOCTL = CALDCO_16MHZ;                    // DCO = 16MHz calibrated
  BCSCTL1 = CALBC1_16MHZ;                   // DCO = 16MHz calibrated

  // Init
  UART_Init();                              // Init software UART
  FM25V40_Init();                           // Init FM25V40 SPI control

  // Mainloop
  for (;;)
  {
    char RXBte;                             // Received Opcode / Write Data

    __bis_SR_register(LPM0_bits + GIE);     // Wait for RX byte

    // A byte has been received...
    P1OUT ^= LedRED;                        // FIX - Debug
    RXBte = RXData;                         // Unload Data before clearing flag
    RXUARTDataValid = 0;                    // RX Data has been read

    if (RXBte & 0xF9)                       // Valid?
      for (;;) P1OUT |= LedRED;             // NO. Signal Error
    else
      switch (__even_in_range(RXBte, 8))    // Use calculated jump table branching
	{
	case  0 :                           // 0x00: Is also an error
	  for (;;) P1OUT |= LedRED;         //  Signal Error.
	case  2 :                           // 0x02: Deassert CS_
	  FM25V40_Stop();                   //  Stop current SPI transaction
	  break;
	case  4 :                           // 0x04: Read
          TXData = FM25V40_Read();          // Read from SPI
	  TX_UART();                        // Send to Host
	  break;
	case  6 :                           // 0x06: Assert CS_ and drive the NEXT byte onto TDO
	  __bis_SR_register(LPM0_bits + GIE);     // Wait for next RX byte
	  RXBte = RXData;                   // Unload
	  RXUARTDataValid = 0;              // RX Data has been read
	  FM25V40_Wrte(RXBte);              // Send it over SPI
	  break;
	}
  }
}

// =============================================================================
// Timer0 A0 interrupt service routine - UART RX
#if defined(__TI_COMPILER_VERSION__)
  #pragma vector=TIMER0_A0_VECTOR
  __interrupt void Timer_A0_ISR (void)
#else
  void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) TIMER_A0_ISR (void)
#endif
{
  TACCR0 += Bitime;                         // Add Offset to CCR0

  if ( TACCTL0 & CAP ) {                    // Capture mode = start bit edge
    TACCTL0 &= ~CAP;                        // Switch from capture to compare mode
    TACCR0 += Bitime50;
  }
  else {
    RXTempData = RXTempData >> 1;
    if (TACCTL0 & SCCI)                     // Get bit waiting in receive latch
      RXTempData |= 0x80;
    RXBitCnt --;
    if ( RXBitCnt == 0)	{                   // All bits RXed?
      RXData = RXTempData;                  // Overwrite previous
      if (RXUARTDataValid)                  // Overrun?
	while (1)                           // Another byte received before the last one taken
	  P1OUT |= LedRED;                  // Signal ERROR
      
      RXUARTDataValid = 1;                  // RXData is valid and not yet unloaded
      RXBitCnt = 8;                         // Re-Load Bit counter for next RX char
      TACCTL0 = SCS + OUTMOD0 + CM1 + CAP + CCIE; // Sync, Neg Edge, Cap
                                            // wait for next falling RX edge
                                            // which is the next start bit
      __bic_SR_register_on_exit(LPM4_bits); // Clear all LPM bits from SR on stack
    }
  }
}

// =============================================================================
// Timer0 A1 interrupt service routine - UART TX
// ??? "Last bit is a bit shorter to give time for CPU processing."
// ??? "For full speed echo TX should be a bit faster than RX to avoid overruns."
#if defined(__TI_COMPILER_VERSION__)
  #pragma vector=TIMER0_A1_VECTOR
  __interrupt void Timer_A1_ISR (void)
#else
  void __attribute__ ((interrupt(TIMER0_A1_VECTOR))) TIMER_A1_ISR (void)
#endif
{
  switch (__even_in_range(TAIV, 10))        // Use calculated jump table branching, and clear highest
  {
    case 2 :                                // TACCR1 CCIFG - UART TXD
      if (TXBitCnt == 0)                    // All bits TXed?
	TACCTL1 &= ~CCIE;                   //  Yes: disable interrupt, signal completion
      else {
	TXBitCnt --;
	TACCTL1 &= ~ OUTMOD2;               // (Assume) SET, TX <= '1' on next interrupt
	if (TXBitCnt == 0) {                // Stop bit next?
	  TACCR1 += Bitime80;               // Yes, and SET was correct
	}
	else {
	  TACCR1 += Bitime99;               // Add Offset to CCR0
	  if (!(TXData & 0x01))
	    TACCTL1 |= OUTMOD2;             // Correct to RESET, TX <= '0' on next interrupt
	}	    
	TXData = TXData >> 1;
      }
      break;
  }
}
