######################################################################
#  Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
#  Warren W. Gay VE3WWG
######################################################################

CC	= gcc
INCL	= -I/usr/local/include
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG) $(INCL)
LDFLAGS = -L/usr/local/lib -lzmq -lncurses -Wl,-R/usr/local/lib

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

all:	sensor console

sensor:	sensor.o
	$(CC) sensor.o -o sensor $(LDFLAGS)
	sudo chown root ./sensor
	sudo chmod u+s ./sensor

console: console.o
	$(CC) console.o -o console $(LDFLAGS)

mac_console: console.o
	$(CC) console.o -o mac_console -L/usr/local/lib -lzmq -lncurses

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f sensor console mac_console

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
