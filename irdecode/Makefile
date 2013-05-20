CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS=irdecode.o

all:	$(OBJS)
	$(CC) $(OBJS) -o irdecode
	sudo chown root ./irdecode
	sudo chmod u+s ./irdecode

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f irdecode

# End
