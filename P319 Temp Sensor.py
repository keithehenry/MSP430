import serial # for serial port

#needs: python2, pyserial, numpy, 
#0) flash the serial temperature measurement program into the msp430
#1) start this program
#2) press the button the Launchpad to enter 'applcation mode'
#3) warm the chip (eg with a light bulb or your fingers)
#4) when you've seen enough, press the reset button on the launchpad
#5) exit the program by pressing 'q' or clicking on the x

#define the serial port. Pick one:
port = "/dev/ttyACM0"  #for Linux
#port = "COM5" #For Windows?
#port = "/dev/tty.uart-XXXX" #For Mac?


#start our program proper:
#open the serial port
try:
    ser = serial.Serial(port,2400,timeout = 0.050) 
    ser.baudrate=9600
# with timeout=0, read returns immediately, even if no data
except:
    print ("Opening serial port",port,"failed")
    print ("Edit program to point to the correct port.")
    print ("Hit enter to exit")
    raw_input()
    quit()

ser.flushInput()

longdata = 0
while(1): #loop forever
    data = ser.read(1) # look for a character from serial port - will wait for up to 50ms (specified above in timeout)
    if len(data) > 0: #was there a byte to read?
        longdata = ((longdata & 0xFF) << 8) + ord(data)
        tempf = ((longdata - 630) * 761) / 1024
        tempc = ((longdata - 673) * 423) / 1024
        if tempf < 140:
            print (hex(longdata), "{:.2f} F".format(tempf), "{:.2f} C".format(tempc))
