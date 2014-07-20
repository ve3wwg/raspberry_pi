######################################################################
#  Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
#  Warren W. Gay VE3WWG
######################################################################

CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)
LDFLAGS	= -lm

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS=unipolar.o

all:	unipolar

unipolar: $(OBJS)
	$(CC) $(OBJS) -o unipolar $(LDFLAGS)
	sudo chown root ./unipolar
	sudo chmod u+s ./unipolar

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f unipolar

unipolar.o: unipolar.c gpio_io.c timed_wait.c

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
