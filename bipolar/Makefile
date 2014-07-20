######################################################################
# Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
# Warren W. Gay VE3WWG
######################################################################

CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)
LDFLAGS	= -lm -lpthread

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS	= bipolar.o

all:	bipolar

bipolar: $(OBJS)
	$(CC) $(OBJS) -o bipolar $(LDFLAGS)
	sudo chown root ./bipolar
	sudo chmod u+s ./bipolar

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f bipolar

bipolar.o: bipolar.c gpio_io.c timed_wait.c

######################################################################
#  End bipolar/Makefile
######################################################################
