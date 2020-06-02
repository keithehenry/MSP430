#include <msp430.h>
#include <intrinsics.h>
#include <stdint.h>

/*
   main.c

   This code reads a DHT22 (AM2303) Digital Humidity & Temperature sensor.
   It is low-power and interrupt driven using TIMER1.CCR2.

   MCLK is sourced from DCO at 8MHz (125 ns).
   - 1MHz was a bit slow due to interrupt latency
   - 8MHz is the next available calibrated frequency
   - 2MHz probably would work fine
   SMCLK is sourced from DCO/8 or 1MHz (1 us).
   - Timer1 clock is SMCLK/8 or 125KHz (8 us)
   ACLK not initialized.

   Using MSP430G2553, 20-pin DIP.
   P2.4 is primary input to TimerA1.CCI2A.
   See your specific MSP430 datasheet for other pin options.

   TIMER1 (overflow) used as RTC/WDT substitute.
   - Some timer must be used to guarantee a minimum 2s delay between queries
   TIMER1 CCR2 used for timing pulse and capture.
   - Generates 1ms LO pulse to start query
   - Switches to capture mode to decode response

    Setup():
        Set up stack and WDT.
        Set clocks and pins.
        Set TIMER1 free running.
        Set TIMER1 as RTC/WDT substitute.

    Loop():
        Sleep until RTC triggers.
        Decode wakeup:

        RTC:
            time to collect data?
            - Call start capture
            time to process data?
            - Process and export

        Start capture:
            Initial interrupt routine state
            Force interrupt

        Timer State:
        Start:
            drive data pin LO
            delay ~1ms
        Wait:
            release data pin
            enable CCR2 capture
        Capture:
            capture raw bits
*/

// Port 1
const unsigned RED = BIT0;
const unsigned RXD = BIT1;
const unsigned TXD = BIT2;
const unsigned SW2 = BIT3;
const unsigned GRN = BIT6;

// Port 2
const unsigned char DHT = BIT4;

const unsigned long smclk_freq = 1000000;       // SMCLK frequency in hertz
const unsigned long bps = 9600;                 // Async serial bit rate

// Output char to UART
static inline void putc(const unsigned c) { while(UCA0STAT & UCBUSY); UCA0TXBUF = c; }

// Output string to UART
void puts(const char *s) { while(*s) putc(*s++); }

// Print unsigned int X 10
void print_ux10(unsigned x)
{
    static const unsigned dd[] = { 10000, 1000, 100, 10, 1 }; // 10000 degrees is overkill
    const unsigned *dp = dd; unsigned d;
    if(x) { while(x < *dp) ++dp;
        do { d = *dp++; char c = '0'; while(x >= d) ++c, x -= d; putc(c); } while(!(d & 2));
    } else putc('0');
    putc('.'); putc('0' + x);
}

// Print signed int X 10
void print_ix10(const int x) { if(x < 0) putc('-'); print_ux10((x < 0) ? -x : x); }

// Print signed c X 10 in degrees F
void print_fx10(const int x) {
    int f;
    f = ((x << 3) + x) + 1603;                       // f*10*5 = c*10*9 + 32*10*5 + 0.05*10*5 (for rounding)
    if (f < 0) {putc('-'); f = -f;}

    static const unsigned dd[] = { 50000, 5000, 500, 50, 5 }; // 1000 degrees is overkill
    const unsigned *dp = dd; unsigned d;
    if(f) { while(f < *dp) ++dp;
        do { d = *dp++; char c = '0'; while(f >= d) ++c, f -= d; putc(c); } while(!(d & 2));
    } else putc('0');
    putc('.');
    d = *dp++; char c = '0'; while(f >= d) ++c, f -= d; putc(c);
}


//// Begin non-UART code
#define TRIGGER_LO 124                                  // (1ms) 1000us / 8us => 125.
#define CAP_HI 12                                       // 12 * 8us => 96us [ (70 to 80) < 96 < (116 to 130)

enum ccr2_t {capture, wait, start};                     // capture = 0 for fastest decode
enum ccr2_t ccr2state __attribute__((noinit));

typedef union {                                         // Implementation dependent non-standard use
    uint64_t ui64;                                      // For bit-shifting
    uint16_t ui16[4];                                   // For humidity and temperature
    uint8_t  ui8[8];                                    // For parity check
} uu6416_t;
uu6416_t cap_dat __attribute__((noinit));               // Generates unusual compiler warning?

uint16_t cap_old __attribute__((noinit));               // Previous capture register data

int16_t temperature __attribute__((noinit));
int16_t humidity __attribute__((noinit));


void main(void) {
    WDTCTL = WDTPW | WDTHOLD;                         // Stop watchdog timer

    // Generic Setup()
  // Set DCO to 8 MHz (0.125 us)
  DCOCTL = 0;
  BCSCTL1 = CALBC1_8MHZ;
  DCOCTL = CALDCO_8MHZ;

  // Set MCLK to DCO/1; SMCLK to DCO/8 => 1 MHz (1 us)
  BCSCTL2 = DIVS_3;

  // Configure Ports 1,2,3 for Pull-up Inputs (for compiler warnings)
//  P1OUT = 0xFF;
//  P1REN = 0xFF;
//  P1DIR = 0x00;
  P2OUT = 0xFF;
  P2REN = 0xFF;
  P2DIR = 0x00;
  P3OUT = 0xFF;
  P3REN = 0xFF;
  P3DIR = 0x00;

  // Customize Pins
  P1OUT = P1REN  = RXD | SW2;                 // Pull Ups for the inputs; LEDs and others are outputs and LO
  P1DIR = ~(RXD | SW2) & 0xFF;                // RXD and SW2 are inputs
  P1SEL = P1SEL2 = RXD | TXD;                 // RXD and TXD connect to UART
                                              //
  P2REN = ~DHT;                                        // Interferes with TIMER input mode. Already has external Pull-up.

  // Nothing is enabled yet anyway
  __enable_interrupt();

  //// Configure Timer1
  // Set TIMER1 to free running continuous up
  TA1CTL = TASSEL_2 | ID_3 | MC_2 | TACLR;              // SMCLK | /8 | continuous | clear

  //// Use Timer1 as RTC substitute
  // 2^16 * 8us => 0.524288s
  TA1CTL |= TAIE;                                       // Enable overflow interrupt

  //// Configure UART for terminal
  const unsigned long brd = (smclk_freq + (bps >> 1)) / bps; // Bit rate divisor
  UCA0CTL1 = UCSWRST;                         // Hold USCI in reset to allow configuration
  UCA0CTL0 = 0;                               // No parity, LSB first, 8 bits, one stop bit, UART (async)
  UCA0BR1 = (brd >> 12) & 0xFF;               // High byte of whole divisor
  UCA0BR0 = (brd >> 4) & 0xFF;                // Low byte of whole divisor
  UCA0MCTL = ((brd << 4) & 0xF0) | UCOS16;    // Fractional divisor, oversampling mode
  UCA0CTL1 = UCSSEL_2;                        // Use SMCLK for bit rate generator, release reset

  puts("\r\nDHT22 Sensor Readings\r\n");

  /// Main loop
  while (1) {
    __low_power_mode_0();                               // Wait 0.524s * 4 all together

    // Compute and compare checksum. (Pipelined.)
    // - First result bogus, must be discarded. Probably checksum mismatch.
    // - Second result very stale, from previous capture, possibly a long time ago.
    // - Third result is from ~2s ago. A new result still being computed in the DHT.
    if (((cap_dat.ui8[2] + cap_dat.ui8[3] + cap_dat.ui8[4] + cap_dat.ui8[5]) & 0xFF) == cap_dat.ui8[1]) {
        humidity    = cap_dat.ui16[2];
        temperature = cap_dat.ui16[1];
        if (temperature < 0) {                          // Fix negative temperature encoding
            temperature ^= 0x7FFF;                      // Make 1's complement
            temperature += 1;                           // And 2's complement
        }
    } else {
        P1OUT |= BIT0;                                  // Red LED ON
    }
    P1OUT ^= BIT6;                                      // Grn LED Toggle (as heartbeat)

    // Print results to serial terminal
    print_ix10(temperature); puts(" °C  ");
    print_fx10(temperature); puts(" °F  ");
    print_ux10(humidity); puts(" %RH\r\n");

    __low_power_mode_0();                               // Wait
    __low_power_mode_0();                               // Wait
    __low_power_mode_0();                               // Wait at least 2s total

    //// Start DHT22 sequence
    // Timer1.2 internal state takes over
    ccr2state = start;
    TA1CCTL2 = CCIE | CCIFG;                            // Force TA1CCTL2 interrupt
    // Could easily take 4ms+. MUST WAIT.

  } // end while

} // end main

//TIMER1 non-CCR0 vector
#pragma vector = TIMER1_A1_VECTOR
  __interrupt void TA1_capture_ISR (void) {

  uint16_t ta1iv;

  ta1iv = TA1IV;                                        // Clear this interrupt. Reading again might clear a differnt interrupt?
  if (ta1iv == 0x04) {
      if (ccr2state == capture) {                       // If MCLK is 1us, latency is an issue. At 8MHz, no problem.
          cap_dat.ui64 <<= 1;
          if ((TA1CCR2 - cap_old) > CAP_HI) {
               cap_dat.ui8[1] |= 0x01;
           }
          cap_old = TA1CCR2;
      } else if (ccr2state == start) {
          // Drive DHT22 data pin LO
          P2SEL &= ~DHT;                                   // Restore to normal input, from TA1.CCI2A mode
          P2OUT &= ~DHT;
          P2DIR |=  DHT;
          // for 1ms
          TA1CCR2 = TRIGGER_LO + TA1R;
          ccr2state = wait;
      } else if (ccr2state == wait) {
          // Turn off drive and reconfig for CCR2 input
          P2DIR &= ~DHT;
          P2SEL |=  DHT;                                   // Config as TimerA1.CCI2A input
          // Reconfig CCR2 for capture mode
          TA1CCTL2 = CM_2 | CCIS_0 | SCS  | CAP | CCIE;     //falling, CCIxA, synchronized, capture, enable (and clear pending)
          ccr2state = capture;
      }
  } else if (ta1iv == 0x0A) {
      __low_power_mode_off_on_exit();
  }
}
