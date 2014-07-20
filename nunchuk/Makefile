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

OBJS=nunchuk.o

all:	$(OBJS)
	$(CC) $(OBJS) -o nunchuk -lm
	sudo chown root ./nunchuk
	sudo chmod u+s ./nunchuk

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f nunchuk

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
