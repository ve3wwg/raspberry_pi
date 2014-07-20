######################################################################
#  Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
#  Warren W. Gay VE3WWG
######################################################################

CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS=rtscts.o

all:	$(OBJS)
	$(CC) $(OBJS) -o rtscts
	sudo chown root ./rtscts
	sudo chmod u+s ./rtscts

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f rtscts

rtscts.o: rtscts.c

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
