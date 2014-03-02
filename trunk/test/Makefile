PROG            = test
CFLAGS		= -m32 -O -Wall -Werror -I..
LDFLAGS		= -m32
OBJS            = ir2.o ik13.o calc.o test.o
VPATH           = ..

#
# Select MK-61 (default) or MK-64.
#
#CFLAGS          += -DMK_54

all:		$(PROG)

$(PROG):        $(OBJS)
		$(CC) $(LDFLAGS) $(OBJS) -o $@

clean:
		rm -f $(PROG) *.o *~ a.out

run:            test test.log
		./test > log
		@diff -q log test.log && echo Test PASSED.

###
ik13.o: ik13.c calc.h
ir2.o: ir2.c calc.h
calc.o: calc.c calc.h ik1302.c ik1303.c
test.o: test.c calc.h
