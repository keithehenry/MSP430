# MSP430
MSP430 code, etc.

### Blink2453wACLKoutUsingVLO.c
Yet another Blink. Ultra-low power blink but with ACLK routed to GRN LED for observation.

### CMeter.c
Capacitor Meter -
Uses a pin interrupt (button), comparator (to 1/4 Vcc), and a timer to measure the discharge time of an attached RC network.

### P319 Full Duplex.c
Full Duple Serial port -
Uses UART (MSP430G2553) to test communication via a terminal emulator, such as PuTTY.

### P319 Temp Sensor.c, P319 Temp Sensor.py
Temperature Sensor -
Uses Python to read the 16-bit onchip temperature sensor (MSP430G2553) and print in degrees F and C.

### PWM at 440 Hz.c
Pulse Width Modulation -
Simple use of timer to create 440 Hz square wave.

### send-one-recv-one.py
Basic Python code to test serial port.
