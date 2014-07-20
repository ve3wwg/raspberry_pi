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

OBJS=tusb.o

all:	$(OBJS)
	$(CC) $(OBJS) -o tusb -lusb

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f tusb

tusb.o: tusb.c

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
