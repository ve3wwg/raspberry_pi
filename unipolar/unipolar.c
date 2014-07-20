/*********************************************************************
 * unipolar.c : Drive unipolar stepper motor
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
#include <signal.h>
#include <assert.h>

#include "gpio_io.c"			/* GPIO routines */
#include "timed_wait.c"			/* timed_wait() */

static const int steps_per_360 = 100;	/* Full steps per rotation */

/*                GPIO Pins:  A   B   C   D */
static const int gpios[] = { 17, 24, 22, 23 };

static float step_time = 0.1;		/* Seconds */
static int drive_mode = 0;		/* Drive mode 0, 1, or 2 */
static int step_no = 0;			/* Step number */
static int steps_per_r = 100;		/* Steps per rotation */
static int position = 0;		/* Stepper position */
static int on_off = 0;			/* Motor drive on/off */

static int quit = 0;			/* Exit program if set */

/*********************************************************************
 * Await so many fractional seconds
 *********************************************************************/
static void
await(float seconds) {
	long sec, usec;
	
	sec = floor(seconds);			/* Seconds to wait */
	usec = floor((seconds-sec)*1000000);	/* Microseconds */
	timed_wait(sec,usec,0);			/* Wait */
}

/*********************************************************************
 * Set motor drive mode
 *********************************************************************/
static void
set_mode(int mode) {
	int micro_steps = mode < 2 ? 1 : 2;

	step_no = 0;
	drive_mode = mode;
	steps_per_r = steps_per_360 * micro_steps;
	printf("Drive mode %d\n",drive_mode);
}

/*********************************************************************
 * Drive all fields according to bit pattern in pins
 *********************************************************************/
static void
drive(int pins) {
	short x;
	for ( x=0; x<4; ++x )
		gpio_write(gpios[x],pins & (8>>x) ? 1 : 0);
}

/*********************************************************************
 * Advance motor:
 *	dir = -1	Step counter-clockwise
 *	dir =  0	Turn on existing fields
 *	dir = +1	Step clockwise
 *********************************************************************/
static void
advance(int dir) {
	static int m0drv[] = { 8,  4,  2, 1 };
	static int m1drv[] = { 9, 12,  6, 3 };
	static int m2drv[] = { 9,  8, 12, 4, 6, 2, 3, 1 };

	switch ( drive_mode ) {
	case 0 :				/* Simple mode 0 */
		step_no = (step_no + dir) & 3;
		drive(m0drv[step_no]);
		await(step_time/4.0);
		break;
	case 1 :				/* Mode 1 drive */
		step_no = (step_no + dir) & 3;
		drive(m1drv[step_no]);
		await(step_time/6.0);
		break;
	case 2 :				/* Mode 2 drive */
		step_no = (step_no + dir) & 7;
		drive(m2drv[step_no]);
		await(step_time/12.0);
		;
	}

	on_off = 1;		/* Mark as drive enabled */
}

/*********************************************************************
 * Move +/- n steps, keeping track of position
 *********************************************************************/
static void
move(int steps) {
	int movement = steps;
	int dir = steps >= 0 ? 1 : -1;
	int inc = steps >= 0 ? -1 : 1;

	for ( ; steps != 0; steps += inc )
		advance(dir);
	position = (position + movement + steps_per_r) % steps_per_r;
}

/*********************************************************************
 * Move to an hour position
 *********************************************************************/
static void
move_oclock(int hour) {
	int new_pos = floor( (float) hour * steps_per_r / 12.0 );
	int diff;

	printf("Moving to %d o'clock.\n",hour);

	if ( new_pos >= position ) {
		diff = new_pos - position;
		if ( diff <= steps_per_r / 2 )
			move(diff);
		else	move(-(position + steps_per_r - new_pos));
	} else	{
		diff = position - new_pos;
		if ( diff <= steps_per_r / 2 )
			move(-diff);
		else	move(new_pos + steps_per_r - position);
	}
}

/*********************************************************************
 * Provide usage info:
 *********************************************************************/
static void
help(void) {
	puts(	"Enter 0-9,A,B for 0-9,10,11 o'clock.\n"
		"'<' to slow motor speed,\n"
		"'>' to increase motor speed,\n"
		"'J','K' or 'L' for modes 0-2,\n"
		"'+'/'-' to step 1 step,\n"
		"'O' to toggle drive on/off,\n"
		"'P' to show position,\n"
		"'Q' to quit.\n");
}

/*********************************************************************
 * Main program
 *********************************************************************/
int
main(int argc,char **argv) {
	int tty = 0;				/* Use stdin */
	struct termios sv_ios, ios;
	int x, rc;
	char ch;

	if ( argc >= 2 )
		drive_mode = atoi(argv[1]);	/* Drive mode 0-2 */

 	rc = tcgetattr(tty,&sv_ios);		/* Save current settings */
	assert(!rc);
	ios = sv_ios;
	cfmakeraw(&ios);			/* Make into a raw config */
	ios.c_oflag = OPOST | ONLCR;		/* Keep output editing */
	rc = tcsetattr(tty,TCSAFLUSH,&ios);	/* Put terminal into raw mode */
	assert(!rc);

	gpio_init();    			/* Initialize GPIO access */
	drive(0);				/* Turn off output */
	for ( x=0; x<4; ++x )
		gpio_config(gpios[x],Output);	/* Set GPIO pin as Output */

	help();

	set_mode(drive_mode);
	printf("Step time: %6.3f seconds\n",step_time);

	while ( !quit ) {
		write(1,": ",2);
		rc = read(tty,&ch,1);	/* Read char */
		if ( rc != 1 ) 
			break;
		if ( islower(ch) )
			ch = toupper(ch);

		write(1,&ch,1);
		write(1,"\n",1);

		switch ( ch ) {
		case 'Q' :			/* Quit */
			quit = 1;
			break;
		case '<' :			/* Go slower */
			step_time *= 2.0;
			printf("Step time: %6.3f seconds\n",step_time);
			break;
		case '>' :			/* Go faster */
			step_time /= 2.0;
			printf("Step time: %6.3f seconds\n",step_time);
			break;
		case '?' :			/* Provide help */
		case 'H' :
			help();
			break;
		case 'J' :			/* Mode 0 */
		case 'K' :			/* Mode 1 */
		case 'L' :			/* Mode 2 */
			move_oclock(0);
			set_mode((int)ch-(int)'J');
			break;
		case 'A' :			/* 10 o'clock */
		case 'B' :			/* 11 o'clock */
			move_oclock((int)ch-(int)'A'+10);
			break;
		case 'O' :			/* Toggle on/off drive */
			on_off ^= 1;
			if ( !on_off )
				drive(0);	/* Turn off motor drive */
			else	advance(0);	/* Re-assert motor drive */
			break;
		case '+' :			/* Advance +1 */
		case '=' :			/* Tread '=' as '+' for convenience */
		case '-' :			/* Counter clock-wise 1 */
			move(ch == '-' ? -1 : 1);
			/* Fall thru */
		case 'P' :			/* Display Position */
			printf("Position: %d of %d\n",position,steps_per_r);
			break;
		default :			/* 0 to 9'oclock */
			if ( ch >= '0' && ch <= '9' )
				move_oclock((int)ch-(int)'0');
			else	write(1,"???\n",4);
		}
	}

	puts("\nExit.");

	drive(0);
	for ( x=0; x<4; ++x )
		gpio_config(gpios[x],Input);	/* Set GPIO pin as Input */

	tcsetattr(tty,TCSAFLUSH,&sv_ios);	/* Restore terminal mode */
	return 0;
}

/*********************************************************************
 * End unipolar.c - by Warren Gay
 * Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/

