OBJECTS=main.o
MAP=main.map

INSTALL_DIR=$(HOME)/ti/msp430-gcc
GCC_DIR = $(INSTALL_DIR)/bin
SUPPORT_FILE_DIRECTORY = $(INSTALL_DIR)/include

DEVICE  = MSP430G2452
CC      = $(GCC_DIR)/msp430-elf-gcc
GDB     = mspdebug rf2500

CFLAGS = -I $(SUPPORT_FILE_DIRECTORY) -mmcu=$(DEVICE) -O3 -Wall -Wno-main -g
LFLAGS = -L $(SUPPORT_FILE_DIRECTORY) -Wl,-Map,$(MAP),--gc-sections 

all: ${OBJECTS}
	$(CC) $(CFLAGS) $(LFLAGS) $? -o $(DEVICE).out

asm:
	$(CC) $(CFLAGS) -fverbose-asm -masm-hex -S main.c

asm2:
	$(CC) $(CFLAGS) -masm-hex -c -g -Wa,-a,-ad main.c > main.lst

clean: 
	$(RM) $(OBJECTS)
	$(RM) $(MAP)
	$(RM) *.out
	$(RM) *.s
	$(RM) *.lst

debug: all
	$(GDB) "prog $(DEVICE).out"
