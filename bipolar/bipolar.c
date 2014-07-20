/*********************************************************************
 * bipolar.c : Drive a bipolar stepper motor
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <termio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <assert.h>

#include "gpio_io.c"			/* GPIO routines */
#include "timed_wait.c"			/* timed_wait() */

/*
 * GPIO definitions :
 */
static const int g_enable	= 17;		/* L298 EnA and EnB */
static const int g_in1		= 27;		/* L298 In1 */
static const int g_in2		= 22;		/* L298 In2 */
static const int g_in3		= 23;		/* L298 In3 */
static const int g_in4		= 24;		/* L298 In4 */

static volatile int stepper_mode = 0;		/* Stepper mode - 1 */
static volatile float step_time	= 0.1;		/* Step time in seconds */

static volatile char cmd	= 0;		/* Thread command when non-zero */
static volatile char stop	= 0;		/* Stop thead when non-zero */
static volatile char stopped	= 0;		/* True when thread has stopped */

static pthread_mutex_t mutex;			/* For inter-thread locks */
static pthread_cond_t cond;			/* For inter-thread signaling */

/*
 * Await so many fractional seconds
 */
static void
await(float seconds) {
	long sec, usec;
	
	sec = floor(seconds);			/* Seconds to wait */
	usec = floor((seconds-sec)*1000000);	/* Microseconds */
	timed_wait(sec,usec,0);			/* Wait */
}

/*
 * Enable/Disable drive to the motor
 */
static inline void
enable(int enable) {
	gpio_write(g_enable,enable);
}

/*
 * Drive the appropriate GPIO outputs :
 */
static void
drive(int L1L2) {
	gpio_write(g_in1,L1L2&0x08);
	gpio_write(g_in2,L1L2&0x04);
	gpio_write(g_in3,L1L2&0x02);
	gpio_write(g_in4,L1L2&0x01);
}

/*
 * Take one step in a direction:
 */
static void
step(int direction) {
	static const int modes[3][8] = {
		{ 0b1000, 0b0010, 0b0100, 0b0001 },	/* Mode 1 */
		{ 0b1010, 0b0110, 0b0101, 0b1001 },	/* Mode 2 */
		{ 0b1000, 0b1010, 0b0010, 0b0110, 0b0100, 0b0101, 0b0001, 0b1001 }
	};
	static int stepno = 0;				/* Last step no. */
	int m = stepper_mode < 2 ? 4 : 8;		/* Max steps for mode */

	if ( direction < 0 )
		direction = m - 1;

	enable(0);					/* Disable motor */
	drive(modes[stepper_mode][stepno]);		/* Change fields */
	enable(1);					/* Drive motor */

	stepno = (stepno+direction) % m;		/* Next step */
}

/*
 * Set the stepper mode of operation:
 */
static inline void
set_mode(int mode) {
	enable(0);	
	stepper_mode = mode;
}

/*
 * Take a command off of the input queue
 */
static char
get_cmd(void) {
	char c;

	pthread_mutex_lock(&mutex);

	while ( !cmd )
		pthread_cond_wait(&cond,&mutex);

	c = cmd;
	cmd = stop = 0;
	pthread_mutex_unlock(&mutex);
	pthread_cond_signal(&cond);		/* Signal that cmd is taken */

	return c;
}

/*
 * Stepper controller thread:
 */
static void *
controller(void *ignored) {
	int command;
	int direction;

	for ( stopped=1;; ) {
		command = get_cmd();
		direction = command == 'F' ? 1 : -1;

		for ( stopped=0; !stop; ) {
			step(direction);
			await(step_time);
		}
		stopped = 1;
	}
	return 0;
}

/*
 * Queue up a command for the controller thread:
 */
static void
queue_cmd(char new_cmd) {

	pthread_mutex_lock(&mutex);		/* Gain exclusive access */

	/* Wait until controller grabs and zeros cmd */
	while ( cmd )
		pthread_cond_wait(&cond,&mutex);

	cmd = new_cmd;				/* Deposit new command */

	pthread_mutex_unlock(&mutex);		/* Unlock */
	pthread_cond_signal(&cond);		/* Signal that cmd is there */
}

/*
 * Stop the current operation:
 */
static void
stop_cmd(void) {
	for ( stop=1; !stopped; stop=1 )
		await(0.100);
}

/*
 * Provide usage info:
 */
static void
help(void) {
	puts(	"Enter:\n"
		"  1 - One phase mode\n"
		"  2 - Two phase mode\n"
		"  3 - Half step mode\n"
		"  R - Toggle Reverse (counter-clockwise)\n"
		"  F - Toggle Forward (clockwise)\n"
		"  S - Stop motor\n"
		"  + - Step forward\n"
		"  - - Step backwards\n"
		"  > - Faster step times\n"
		"  < - Slower step times\n"
		"  ? - Help\n"
		"  Q - Quit\n");
}

/*
 * Main program
 */
int
main(int argc,char **argv) {
	pthread_t tid;				/* Thread id */
	int tty = 0;				/* Use stdin */
	struct termios sv_ios, ios;
	int rc, quit;
	char ch, lcmd = 0;

 	rc = tcgetattr(tty,&sv_ios);		/* Save current settings */
	assert(!rc);
	ios = sv_ios;
	cfmakeraw(&ios);			/* Make into a raw config */
	ios.c_oflag = OPOST | ONLCR;		/* Keep output editing */
	rc = tcsetattr(tty,TCSAFLUSH,&ios);	/* Put terminal into raw mode */
	assert(!rc);

	/*
	 * Initialize and configure GPIO pins :
	 */
	gpio_init();
	gpio_config(g_enable,Output);
	gpio_config(g_in1,Output);
	gpio_config(g_in2,Output);
	gpio_config(g_in3,Output);
	gpio_config(g_in4,Output);

	enable(0);				/* Turn off output */
	set_mode(0);				/* Default is one phase mode */

	help();

	pthread_mutex_init(&mutex,0);		/* Mutex for inter-thread locking */
	pthread_cond_init(&cond,0);		/* For inter-thread signaling */
	pthread_create(&tid,0,controller,0);	/* The thread itself */

	/*
	 * Process single character commands :
	 */
	for ( quit=0; !quit; ) {
		/*
		 * Prompt and read input char :
		 */
		write(1,": ",2);
		rc = read(tty,&ch,1);
		if ( rc != 1 ) 
			break;
		if ( islower(ch) )
			ch = toupper(ch);

		write(1,&ch,1);
		write(1,"\n",1);

		/*
		 * Process command char :
		 */
		switch ( ch ) {
		case '1' :			/* One phase mode */
			stop_cmd();
			set_mode(0);
			break;
		case '2' :			/* Two phase mode */
			stop_cmd();
			set_mode(1);
			break;
		case '3' :			/* Half step mode */
			stop_cmd();
			set_mode(2);
			break;
		case '<' :			/* Make steps slower */
			step_time *= 2.0;
			printf("Step time is now %.3f ms\n",step_time*1000.0);
			break;
		case '>' :			/* Make steps faster */
			step_time /= 2.0;
			printf("Step time is now %.3f ms\n",step_time*1000.0);
			break;
		case 'F' :			/* Forward: run motor */
			if ( !stopped && lcmd != 'R' ) {
				stop_cmd();	/* Stop due to toggle */
				lcmd = 0;
			} else	{
				stop_cmd();	/* Stop prior to change direction */
				queue_cmd(lcmd='F');
			}
			break;
		case 'R' :			/* Reverse: run motor */
			if ( !stopped && lcmd != 'F' ) {
				stop_cmd();
				lcmd = 0;
			} else	{
				stop_cmd();
				queue_cmd(lcmd='R');
			}
			break;
		case 'S' :			/* Just stop */
			stop_cmd();
			break;
		case '+' :			/* Step clockwise */
		case '=' :			/* So we don't have to shift for + */
			stop_cmd();
			step(1);
			break;
		case '-' :			/* Step counter-clockwise */
			stop_cmd();
			step(-1);
			break;
		case 'Q' :			/* Quit */
			quit = 1;
			break;
		default :			/* Unsupported */
			stop_cmd();
			help();
		}
	}

	stop_cmd();
	enable(0);

	puts("\nExit.");

	tcsetattr(tty,TCSAFLUSH,&sv_ios);	/* Restore terminal mode */
	return 0;
}

/*********************************************************************
 * End bipolar.c
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
