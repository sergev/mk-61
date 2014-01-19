P               = /usr/local/pic32-tools/bin/pic32-
CC              = $(P)gcc -mips32r2 -g -nostdlib
OBJCOPY         = $(P)objcopy
OBJDUMP         = $(P)objdump
SIZE            = $(P)size
#GDB             = /mips/arch/overflow/codesourcery/mips-sde-elf/lite/release/2012.03-64/Linux/bin/mips-sde-elf-gdb
#GDB             = mipsel-elf32-gdb
#GDB             = /usr/local/mips/insight681/bin/mipsel-elf32-insight
#GDB             = /usr/local/mips/insight70/bin/mips-elf-insight
GDB             = /usr/local/mips-gcc-4.7.2/bin/mips-elf-gdb
CFLAGS          = -O3 -Wall -Werror
LDFLAGS         = -e _start

OBJS            = ir2.o ik13.o calc.o

#
# Select MK-61 (default) or MK-64.
#
#CFLAGS          += -DMK_54

#
# Use crystal 12 MHz.
#
CFLAGS          += -DCRYSTAL_12MHZ

#
# DIY board with pic32mx1 processor.
#
#PROG            = mx1
#CFLAGS          += -DPIC32MX2
#OBJS            += mx1.o
#LDFLAGS         += -T mx2-ram8k.ld

#
# DIY board with pic32mx2 processor and USB port.
#
PROG            = mx2-usb
CFLAGS          += -DPIC32MX2
OBJS            += mx2-usb.o usb-device.o usb-function-hid.o
LDFLAGS         += -T mx2-ram8k.ld

#
# Olimex T795 board with pic32mx7 processor.
#
#PROG            = mx7-olimex
#CFLAGS          += -DPIC32MX7
#OBJS            += mx7-olimex.o
#LDFLAGS         += -T mx7-bootloader.ld

all:            $(PROG).hex
		@$(MAKE) -C pmktool $@
		$(SIZE) $(PROG).elf

$(PROG).hex:   $(OBJS)
		$(CC) $(LDFLAGS) $(OBJS) -o $(PROG).elf
		$(OBJCOPY) -O ihex $(PROG).elf $@
		$(OBJDUMP) -mmips:isa32r2 -d -S $(PROG).elf > $(PROG).dis

load:           $(PROG).hex
		pic32prog $(PROG).hex

clean:
		rm -f *.o *.lst *~ *.elf $(PROG).hex *.dis test $(PROG)
		@$(MAKE) -C pmktool $@

debug:          $(PROG).hex
		$(GDB) $(PROG).elf

###
calc.o: calc.c calc.h ik1302.c ik1303.c
ik13.o: ik13.c calc.h
ir2.o: ir2.c calc.h
mx1.o: mx1.c calc.h pic32mx.h
mx2-usb.o: mx2-usb.c calc.h pic32mx.h usb-ch9.h usb-hal-pic32.h usb-device.h usb-function-hid.h
mx7-olimex.o: mx7-olimex.c calc.h pic32mx.h
usb-device.o: usb-device.c pic32mx.h usb-ch9.h usb-hal-pic32.h usb-device.h
usb-function-hid.o: usb-function-hid.c usb-device.h usb-ch9.h usb-hal-pic32.h usb-function-hid.h
