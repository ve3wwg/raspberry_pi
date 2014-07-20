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

all:	ds1307set ds1307get

ds1307set: ds1307set.o
	$(CC) ds1307set.o -o ds1307set
	sudo chown root ./ds1307set
	sudo chmod u+s ./ds1307set

ds1307get: ds1307get.o
	$(CC) ds1307get.o -o ds1307get
	sudo chown root ./ds1307get
	sudo chmod u+s ./ds1307get

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f ds1307set ds1307get

ds1307set.o: ds1307set.c i2c_common.c ds1307.h
ds1307get.o: ds1307get.c i2c_common.c ds1307.h

######################################################################
#  End Makefile. Public Domain License.
######################################################################
