######################################################################
#  Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
#  Warren W. Gay VE3WWG
######################################################################

CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS=dht11.o

all:	$(OBJS)
	$(CC) $(OBJS) -o dht11
	sudo chown root ./dht11
	sudo chmod u+s ./dht11

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f dht11

dht11.o: dht11.c gpio_io.c timed_wait.c

######################################################################
#  End Makefile. Public domain license.
######################################################################
