import serial # for serial port

port = "/dev/ttyACM0"  #for Linux

try:
    ser = serial.Serial(port,4800,timeout = 0.100) 
# with timeout=0, read returns immediately, even if no data
except:
    print ("Opening serial port",port,"failed")
    print ("Hit enter to exit")
    raw_input()
    quit()

ser.flushInput()

while (True):
    s = input("Hex byte: ")
    if s != "":
        ser.write(bytearray.fromhex(s))
    data = ser.read(1) # look for a character from serial port
    if len(data) > 0:  # was there a byte to read?
        print (hex(ord(data)))
