######################################################################
#  Assimilating the Raspberry Pi, by Warren Gay VE3WWG
######################################################################

CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CLFAGS	= $(OPTS) $(DBG)

.c.o:
	$(CC) -c $(CFLAGS) $(OPTS) $(DBG) $< -o $*.o

OBJS=pullup.o

all:	$(OBJS)
	$(CC) $(OBJS) -o pullup
	sudo chown root ./pullup
	sudo chmod u+s ./pullup

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f pullup

pullup.o: pullup.c

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
