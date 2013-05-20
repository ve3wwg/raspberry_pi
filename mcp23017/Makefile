CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS=mcp23017.o

all:	$(OBJS)
	$(CC) $(OBJS) -o mcp23017
	sudo chown root ./mcp23017
	sudo chmod u+s ./mcp23017

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f mcp23017

mcp23017.o: mcp23017.c sysgpio.c 

# End
