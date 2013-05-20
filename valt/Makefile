######################################################################
#  Assimilating the Raspberry Pi, by Warren Gay VE3WWG
######################################################################

CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS	= valt.o

all:	$(OBJS)
	$(CC) $(OBJS) -o valt
	sudo chown root ./valt
	sudo chmod u+s ./valt

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f valt

valt.o: valt.c

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
