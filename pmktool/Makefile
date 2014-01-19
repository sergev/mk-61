CC              = gcc

SVNVERS         = $(shell svnversion)
UNAME           = $(shell uname)
CFLAGS          = -Wall -g -O -Ihidapi -DSVNVERSION='"$(SVNVERS)"'
LDFLAGS         = -g

# Linux
ifeq ($(UNAME),Linux)
    LIBS        += -lusb-1.0 -lpthread
    HIDSRC      = hidapi/hid-libusb.c
endif

# Mac OS X
ifeq ($(UNAME),Darwin)
    LIBS        += -framework IOKit -framework CoreFoundation
    HIDSRC      = hidapi/hid-mac.c
endif

PROG_OBJS       = pmktool.o device.o hid.o opcodes.o parse.o

all:            pmktool

pmktool:        $(PROG_OBJS)
		$(CC) $(LDFLAGS) -o $@ $(PROG_OBJS) $(LIBS)

hid.o:          $(HIDSRC)
		$(CC) $(CFLAGS) -c -o $@ $<

pmktool.po:	*.c
		xgettext --from-code=utf-8 --keyword=_ pmktool.c -o $@

pmktool-ru.mo:  pmktool-ru.po
		msgfmt -c -o $@ $<

pmktool-ru-cp866.mo ru/LC_MESSAGES/pmktool.mo: pmktool-ru.po
		iconv -f utf-8 -t cp866 $< | sed 's/UTF-8/CP866/' | msgfmt -c -o $@ -
		cp pmktool-ru-cp866.mo ru/LC_MESSAGES/pmktool.mo

clean:
		rm -f *~ *.o core pmktool adapter-mpsse pmktool.po

install:	pmktool #pmktool-ru.mo
		install -c -s pmktool /usr/local/bin/pmktool
#		install -c -m 444 pmktool-ru.mo /usr/local/share/locale/ru/LC_MESSAGES/pmktool.mo
###
device.o: device.c device.h hidapi/hidapi.h
opcodes.o: opcodes.c
parse.o: parse.c
pmktool.o: pmktool.c device.h localize.h
