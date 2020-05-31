# MSP430
MSP430 code, etc.

### Blink2453wACLKoutUsingVLO.c - Low-power Blink
Ultra-low power blink using VLK with ACLK routed to GRN LED for observation.

### CMeter.c - Capacitor Meter
Uses a pin interrupt (button), comparator (to 1/4 Vcc), and a timer to measure the discharge time of an attached RC network.

### FM25V40.c - FM25V40 BoosterPack
Test electrical connections and verify basic functionality of the FM25V40 BoosterPack for TI LaunchPad with a MSP430G2453.

### P319 Full Duplex.c - Full Duplex Serial Port
Uses UART (MSP430G2553) to test communication via a terminal emulator, such as PuTTY.

### P319 Temp Sensor.c, P319 Temp Sensor.py - Temperature Sensor
Uses Python to read the 16-bit onchip temperature sensor (MSP430G2553) and print in degrees F and C.

### PWM at 440 Hz.c - Pulse Width Modulation
Simple use of timer to create 440 Hz square wave.

### send-one-recv-one.py - Serial Port
Basic Python code to test serial port.
