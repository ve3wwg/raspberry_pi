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

OBJS=pads.o

all:	$(OBJS)
	$(CC) $(OBJS) -o pads
	sudo chown root ./pads
	sudo chmod u+s ./pads

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f pads

pads.o: pads.c

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
