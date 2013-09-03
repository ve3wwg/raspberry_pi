CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)
LDFLAGS	= -lm -lpthread

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

OBJS	= pcd8544.o

all:	pcd8544

pcd8544: $(OBJS)
	$(CC) $(OBJS) -o pcd8544 $(LDFLAGS)
	sudo chown root ./pcd8544
	sudo chmod u+s ./pcd8544

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f pcd8544

pcd8544.o: pcd8544.c gpio_io.c # timed_wait.c

