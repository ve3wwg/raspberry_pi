/*********************************************************************
 * irdecode.c : Read IR remote control on GPIO 17 (GEN0)
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <getopt.h>

static int gpio_inpin = 17;	/* GPIO input pin */
static int is_signaled = 0;	/* Exit program if signaled */
static int gpio_fd = -1;	/* Open file descriptor */

static jmp_buf jmp_exit;

typedef enum {
	gp_export=0,	/* /sys/class/gpio/export */
	gp_unexport,	/* /sys/class/gpio/unexport */
	gp_direction,	/* /sys/class/gpio%d/direction */
	gp_edge,	/* /sys/class/gpio%d/edge */
	gp_value	/* /sys/class/gpio%d/value */
} gpio_path_t;

/*
 * Samsung Remote Codes :
 */
#define IR_POWER	0xE0E040BF
#define IR_0		0xE0E08877
#define IR_1		0xE0E020DF
#define IR_2		0xE0E0A05F
#define IR_3		0xE0E0609F
#define IR_4		0xE0E010EF
#define IR_5		0xE0E0906F
#define IR_6		0xE0E050AF
#define IR_7		0xE0E030CF
#define IR_8		0xE0E0B04F
#define IR_9		0xE0E0708F
#define IR_EXIT		0xE0E0B44B
#define IR_RETURN	0xE0E01AE5
#define IR_MUTE		0xE0E0F00F

static struct {
	unsigned long	ir_code;	/* IR Code */
	const char 	*text;		/* Display text */
} ir_codes[] = {
	{ IR_POWER,	"\n<POWER>\n" },
	{ IR_0, 	"0" },
	{ IR_1, 	"1" },
	{ IR_2, 	"2" },
	{ IR_3, 	"3" },
	{ IR_4, 	"4" },
	{ IR_5, 	"5" },
	{ IR_6, 	"6" },
	{ IR_7, 	"7" },
	{ IR_8, 	"8" },
	{ IR_9, 	"9" },
	{ IR_EXIT,	"\n<EXIT>\n" },
	{ IR_RETURN,	"\n<RETURN>\n" },
	{ IR_MUTE,	"\n<MUTE>\n" },
	{ 0, 		0 }		/* End marker */
};

/*
 * Compute the time difference in milliseconds:
 */
static double
msdiff(struct timeval *t1,struct timeval *t0) {
	unsigned long ut;
	double ms;

	ms = (t1->tv_sec - t0->tv_sec) * 1000.0;
	if ( t1->tv_usec > t0->tv_usec )
		ms += (t1->tv_usec - t0->tv_usec) / 1000.0;
	else	{
		ut = t1->tv_usec + 1000000UL;
		ut -= t0->tv_usec;
		ms += ut / 1000.0;
	}
	return ms;
}

/*
 * Create a pathname for type in buf.
 */
static const char *
gpio_setpath(int pin,gpio_path_t type,char *buf,unsigned bufsiz) {
	static const char *paths[] = {
		"export", "unexport", "gpio%d/direction",
		"gpio%d/edge", "gpio%d/value" };
	int slen;

	strncpy(buf,"/sys/class/gpio/",bufsiz);
	bufsiz -= (slen = strlen(buf));
	snprintf(buf+slen,bufsiz,paths[type],pin);
	return buf;
}

/*
 * Open /sys/class/gpio%d/value for edge detection :
 */
static int
gpio_open_edge(int pin,const char *edge) {
	char buf[128];
	FILE *f;
	int fd;

	/* Export pin : /sys/class/gpio/export */
	gpio_setpath(pin,gp_export,buf,sizeof buf);
	f = fopen(buf,"w");
	assert(f);
	fprintf(f,"%d\n",pin);
	fclose(f);

	/* Direction :	/sys/class/gpio%d/direction */
	gpio_setpath(pin,gp_direction,buf,sizeof buf);
	f = fopen(buf,"w");
	assert(f);
	fprintf(f,"in\n");
	fclose(f);

	/* Edge :	/sys/class/gpio%d/edge */
	gpio_setpath(pin,gp_edge,buf,sizeof buf);
	f = fopen(buf,"w");
	assert(f);
	fprintf(f,"%s\n",edge);
	fclose(f);

	/* Value :	/sys/class/gpio%d/value */
	gpio_setpath(pin,gp_value,buf,sizeof buf);
	fd = open(buf,O_RDWR);
	return fd;
}

/*
 * Close (unexport) GPIO pin :
 */
static void
gpio_close(int pin) {
	char buf[128];
	FILE *f;

	/* Unexport :	/sys/class/gpio/unexport */
	gpio_setpath(pin,gp_unexport,buf,sizeof buf);
	f = fopen(buf,"w");
	assert(f);
	fprintf(f,"%d\n",pin);
	fclose(f);
}

/*
 * This routine will block until the open GPIO pin has changed
 * value.
 */
static int
gpio_poll(int fd,double *ms) {
	static char needs_init = 1;
	static struct timeval t0;
	static struct timeval t1;
	struct pollfd polls;
	char buf[32];
	int rc, n;
	
	if ( needs_init ) {
		rc = gettimeofday(&t0,0);
		assert(!rc);
		needs_init = 0;
	}

	polls.fd = fd;			/* /sys/class/gpio17/value */
	polls.events = POLLPRI;		/* Exceptions */
	
	do	{
		rc = poll(&polls,1,-1);	/* Block */
		if ( is_signaled )
			longjmp(jmp_exit,1);
	} while ( rc < 0 && errno == EINTR );
	
	assert(rc > 0);

	rc = gettimeofday(&t1,0);
	assert(!rc);

	*ms = msdiff(&t1,&t0);

	lseek(fd,0,SEEK_SET);
	n = read(fd,buf,sizeof buf);	/* Read value */
	assert(n>0);
	buf[n] = 0;

	rc = sscanf(buf,"%d",&n);
	assert(rc==1);

	t0 = t1;			/* Save for next call */
	return n;			/* Return value */
}

/*
 * Signal handler to quit the program :
 */
static void
sigint_handler(int signo) {
	is_signaled = 1;		/* Signal to exit program */
}

/*
 * Wait until the line changes:
 */
static inline int
wait_change(double *ms) {
	/* Invert the logic of the input pin */
	return gpio_poll(gpio_fd,ms) ? 0 : 1;
}

/*
 * Wait until line changes to "level" :
 */
static int
wait_level(int level) {
	int v;
	double ms;

	while ( (v = wait_change(&ms)) != level )
		;
	return v;
}

/*
 * Get a 32 bit code from remote control:
 */
static unsigned long
getword(void) {
	static struct timeval t0 = { 0, 0 };
	static unsigned long last = 0;
	struct timeval t1;
	double ms;
	int v, b, count;
	unsigned long word = 0;

Start:	word = 0;
	count = 0;

	/*
	 * Wait for a space of 46 ms :
	 */
	do	{
		v = wait_change(&ms);
	} while ( ms < 46.5 );

	/*
	 * Wait for start: 4.5ms high, then 4.5ms low :
	 */
	for ( v=1;;) {
		if ( v )
			v = wait_level(0);
		v = wait_level(1);
		v = wait_change(&ms);	/* High to Low */
		if ( !v && ms >= 4.0 && ms <= 5.0 ) {
			v = wait_change(&ms);
			if ( v && ms >= 4.0 && ms <= 5.0 )
				break;
		}
	}
				
	/*
	 * Get 32 bit code :
	 */
	do	{
		/* Wait for line to go low */
		v = wait_change(&ms);
		if ( v || ms < 0.350 || ms > 0.8500 )
			goto Start;

		/* Wait for line to go high */
		v = wait_change(&ms);
		if ( !v || ms < 0.350 || ms > 2.0 )
			goto Start;

		b =  ms < 1.000 ? 0 : 1;
		word = (word << 1) | b;
	} while ( ++count < 32 );

	/*
	 * Eliminate key stutter :
	 */
	gettimeofday(&t1,0);
	if ( word == last && t0.tv_sec && msdiff(&t1,&t0) < 1100.0 )
		goto Start;		/* Too soon */

	t0 = t1;
	fprintf(stderr,"CODE %08lX\n",word); 
	return word;
}

/*
 * Get text form of remote key :
 */
static const char *
getircode(void) {
	unsigned long code;
	int kx;

	for (;;) {
		code = getword();
		for ( kx=0; ir_codes[kx].text; ++kx )
			if ( ir_codes[kx].ir_code == code )
				return ir_codes[kx].text;
	}
}

/*
 * Main program :
 */
int
main(int argc,char **argv) {
	const char *key;
	int optch;
	int f_dump = 0, f_gnuplot = 0, f_noinvert = 0;

	while ( (optch = getopt(argc,argv,"dgnsp:h")) != EOF )
		switch ( optch ) {
		case 'd' :
			f_dump = 1;
			break;
		case 'g' :
			f_gnuplot = 1;
			break;
		case 'n' :
			f_noinvert = 1;
			break;
		case 'p' :
			gpio_inpin = atoi(optarg);
			break;
		case 'h' :
			/* Fall thru */
		default :
usage:			fprintf(stderr,
				"Usage: %s [-d] [-g] [-n] [-p gpio]\n",argv[0]);
			fputs("where:\n"
				"  -d\t\tdumps events\n"
				"  -g\t\tgnuplot waveforms\n"
				"  -n\t\tdon't invert GPIO input\n"
				"  -p gpio\tGPIO pin to use (17)\n",
				stderr);
			exit(1);
		}

	if ( gpio_inpin < 0 || gpio_inpin >= 32 )
		goto usage;

	if ( setjmp(jmp_exit) )
		goto xit;

	signal(SIGINT,sigint_handler);			/* Trap on SIGINT */
	gpio_fd = gpio_open_edge(gpio_inpin,"both");	/* GPIO input */

	printf("Monitoring GPIO %d for changes:\n",gpio_inpin);

	if ( !f_dump ) {
		/*
		 * Remote control read loop :
		 */
		for (;;) {
			key = getircode();
			fputs(key,stdout);
			if ( !strcmp(key,"\n<EXIT>\n") )
				break;
			fflush(stdout);
		}
	} else	{
		/*
		 * Dump out IR level changes
		 */
		int v;
		double ms, t=0.0;

		wait_change(&ms);		/* Wait for first change */

		for (;;) {
			v = wait_change(&ms) ^ f_noinvert;
			if ( !f_gnuplot )
				printf("%12.3f\t%d\n",ms,v);
			else	{
				printf("%12.3f\t%d\n",t,v^1);
				t += ms;
				printf("%12.3f\t%d\n",t,v^1);
				printf("%12.3f\t%d\t%12.3f\n",t,v,ms);
			}
		}
	}

xit:	fputs("\nExit.\n",stdout);
	close(gpio_fd);				/* Close gpio%d/value */
	gpio_close(gpio_inpin);			/* Unexport gpio */
	return 0;
}

/*********************************************************************
 * End irdecode.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
