######################################################################
#  Assimilating the Raspberry Pi, by Warren Gay VE3WWG
######################################################################

CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS=evinput.o

all:	$(OBJS)
	$(CC) $(OBJS) -o evinput
	sudo chown root ./evinput
	sudo chmod u+s ./evinput

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f evinput

######################################################################
#  End Makefile. Public Domain License.
######################################################################
