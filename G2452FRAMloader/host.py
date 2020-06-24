import serial # for serial port

port = "/dev/ttyACM0"  #for Linux

try:
    ser = serial.Serial(port,9600,timeout = 0.100) 
# with timeout=0, read returns immediately, even if no data
except:
    print ("Opening serial port",port,"failed")
    quit()

ser.reset_input_buffer()

while (True):
    s = input("Hex byte: ")
    if s != "":
        ser.write(bytearray.fromhex(s))
        print ("Sent:", s)
    data = ser.read(1) # look for a character from serial port
    if len(data) > 0:  # was there a byte to read?
        print (hex(ord(data)))
