# MSP430
MSP430 code, etc.

### CMeter.c
Capacitor Meter -
Uses a pin interrupt (button), comparator (to 1/4 Vcc), and a timer to measure the discharge time of an attached RC network.

### P319 Full Duplex.c
Serial port test -
Uses MSP430G2553 UART to test communication via a terminal emulator, such as PuTTY.

### P319 Temp Sensor Sensor.c
### P319 Temp Sensor Sensor.py
Onboard temperature sensor -
Uses Python to read the 16-bit onchip temperature sensor (MSP430G2553) and print in F and C.

