/* KEH 2020-05-27:
FM25V40.c, MSP30G2452 version.

Experimental code to investigate SPI and F-Ram BoosterPack.

Notes:
The naming conventions and options for built-in SPI controllers is confusing:
  The 2452 has USI (USI) but not UCA (USCI).
  The 2553 has USCI with UCA (has IR and better UART) and UCB, but not USI.
  This code is written for the 2452, the less capable of the two.

The BoosterPack is wired to use the usual default pins for USCI_B0.
For the 2452, with USI but not USCI, P1.6 and P1.7 are reversed. Using
SPI in 3-2ire mode, jumpering P1.6 and P1.7 together in this case, should work
with careful use of USIOE (3-state control for SDO, P1.6).

The FM25V40 operates up to 40MHz, so its clock speed is not an issue.
"After a PUC, MCLK and SMCLK are sourced from DCOCLK at ~1.1 MHz... and
 ACLK is sourced from LFXT1CLK...". No XTL on launchpad so ACLK is not sourced.
Default clocks seem fine. Don't use ACLK or reconfigure.

Recommend to remove P1.6 jumper connection to GRN LED.

*/

#include <msp430.h>

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

#define     RDSR    0x05                // Read Status Register
#define     RDID    0x9F                // Read Device ID

// See results in memory (at 0x0200), instead of the stack:
unsigned char rdsr;
unsigned char rdid[9];

int main(void)
{
  volatile unsigned int i;
  
  /*
  Using MCLK and SMCLK from PUC of DCO at ~1.1MHz.
  These two settings select VLO to ACLK. ACLK seems undefined without XTL in place?
  Selecting ACLK <= VCO also turns of the LFXT1 oscillator, saving power.
  VLO is typically 12kHz, 4 min, 20 max.
  POR and/or PUC initialize all the Basic Clock Module+ Registers. DIVAx <= 0.
  */
  /* Not directly related to FM25V40 */
  BCSCTL3 |= LFXT1S_2;                  // ACLK souced from VLO, and disable LFXT1 osc
  WDTCTL = WDTPW + WDTHOLD;             // Stop watchdog timer

  /* Begin FM25V40 code */
  // P1.x:
  P1SEL = SCK | MISO | MOSI;            // Let USIP.x control these pins
  P1OUT = 0xFF;                         // Drive hi. DON't use P1REN for pull-ups!
  P1DIR = 0xFF;                         // Out unless otherwise
  // P2.x:
  P2OUT = 0xFF | CS_;                   // Turn off CS_
  P2DIR = 0xFF;                         // All bits driven

  /* SPI mode 0  {USICKPH, USICKPL} <= 0b10, SCK inactive LO, capture on next edge, change on next edge */
//  USICTL1 &= ~USII2C;                 // (default) Select SPI mode
//  USICKCTL &= ~USICKPL;               // (default) SCK inactive LO
  USICTL1 |= USICKPH;                   // Capture on 1st edge, change on following
  /* SPI mode 3 would be {USICKPH, USICKPL} <= 0b01, as F-RAM always captures on rising edge */
  
  // P1 bits 7, 6, and 5 enable; Master; and (default) hold in reset
  USICTL0 |= USIPE7 + USIPE6 + USIPE5 + USIMST + USISWRST; // Port, SPI master

  USICKCTL |= USISSEL_2;                // /1 SMCLK. Anything but SCLK is possible (SPI mode)
  
  USICTL0 &= ~USISWRST;                 // USI released for operation
  
/*
Method:
Assert CS_.
Read:
  Set USIOE in USICTL0.
  Write Opc to USISRL.
  Load USICNT with 8
  Clr USIOE.
  Load USICNT with 8, read USISRL, repeat.
Write:
  Set USIOE.
  Write Opc to USISRL
  Load data to USISRL and load USICTN with 8, repeat.
Deassert CS_.
*/
    /* A dummy read appears necessary, to wake from Sleep Mode presumably */
    // Initialize:
    for (i = 256; i > 0; i--);            // Wait for Tpu (1ms) [256 * ~4us / loop]
    P2OUT &= ~CS_;                        // Assert CS_
    for (i = 128; i > 0; i--);            // Wait for Trec (450us)
    USICNT = 8;                           // Dummy read
    while (!(USICTL1 & USIIFG)) {}        // Wait for completion
    P2OUT |= CS_;                         // Deassert CS_
    
    // Read chip status
    P2OUT &= ~CS_;                        // Assert CS_
    USICTL0 |= USIOE;                     // SDO enable
    USISRL = RDSR;                        // opcode
    USICNT = 8;                           // Send it
    while (!(USICTL1 & USIIFG)) {}        // Wait
    USICTL0 &= ~USIOE;                    // SDO disable

    USICNT = 8;                           // Read
    while (!(USICTL1 & USIIFG)) {}        // Wait
    rdsr = USISRL;                        // 0x40 is expected result
    P2OUT |= CS_;                         // Deassert CS_
    
    // Read chip ID
    P2OUT &= ~CS_;                        // Assert CS_
    USICTL0 |= USIOE;                     // SDO enable
    USISRL = RDID;                        // opcode
    USICNT = 8;                           // Send it
    while (!(USICTL1 & USIIFG)) {}        // Wait
    USICTL0 &= ~USIOE;                    // SDO disable

    for (i = 9; i-- > 0; ) {
        USICNT = 8;                           // Read
        while (!(USICTL1 & USIIFG)) {}        // Wait
        rdid[i] = USISRL;                     // Collect data (0x7F7F7F7F7F7FC22640)
    }
    P2OUT |= CS_;                         // Deassert CS_

  for (;;) {
    // BREAK
    __no_operation();
  }
}

/*
// USI interrupt service routine
#pragma vector=USI_VECTOR
__interrupt void universal_serial_interface(void)
{
}
*/
