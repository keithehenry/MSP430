import math

PI = 3.14159265
SAMPLES = 32
PRECISION = 256
AMPLITUDE = PRECISION*15/32
#OFFSET=PRECISION/2+7
OFFSET=PRECISION/2

for i in range(0, int(SAMPLES)):
    print (round(math.sin(2*PI*((i+0.5)/SAMPLES))*AMPLITUDE+OFFSET), end=", ")

           
