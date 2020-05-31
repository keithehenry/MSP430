# MSP430
MSP430 code, etc.

### CMeter.c
Capacitor Meter -
Uses a pin interrupt (button), comparator (to 1/4 Vcc), and a timer to measure the discharge time of an attached RC network.

### P319 Full Duplex.c
Full Duple Serial port -
Uses UART (MSP430G2553) to test communication via a terminal emulator, such as PuTTY.

### P319 Temp Sensor.c, P319 Temp Sensor.py
Temperature Sensor -
Uses Python to read the 16-bit onchip temperature sensor (MSP430G2553) and print in degrees F and C.
