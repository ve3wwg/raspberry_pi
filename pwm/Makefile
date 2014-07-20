######################################################################
# Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
# Warren W. Gay VE3WWG
######################################################################

CC	= gcc
OPTS	= -Wall
DBG	= -O0 -g
CFLAGS	= $(OPTS) $(DBG)
LDFLAGS	= -lm 

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

all:	pwm softpwm

pwm:	pwm.o
	$(CC) pwm.o -o pwm $(LDFLAGS)
	sudo chown root ./pwm
	sudo chmod u+s ./pwm

softpwm: softpwm.o
	$(CC) softpwm.o -o softpwm $(LDFLAGS) -lpthread
	sudo chown root ./softpwm
	sudo chmod u+s ./softpwm

clean:
	rm -f *.o core errs.t

clobber: clean
	rm -f pwm softpwm

######################################################################
#  End Makefile.  Public Domain license.
######################################################################
