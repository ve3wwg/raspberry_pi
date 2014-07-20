/*********************************************************************
 * evinput.c : Event driven GPIO input
 *
 * ./evinput gpio#
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <sys/poll.h>

static int gpio_inpin = -1;	/* GPIO input pin */
static int is_signaled = 0;	/* Exit program if signaled */

typedef enum {
	gp_export=0,	/* /sys/class/gpio/export */
	gp_unexport,	/* /sys/class/gpio/unexport */
	gp_direction,	/* /sys/class/gpio%d/direction */
	gp_edge,	/* /sys/class/gpio%d/edge */
	gp_value	/* /sys/class/gpio%d/value */
} gpio_path_t;

/*
 * Internal : Create a pathname for type in buf.
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
 * value. This pin should be connected to the MCP23017 /INTA
 * pin.
 */
static int
gpio_poll(int fd) {
	struct pollfd polls;
	char buf[32];
	int rc, n;
	
	polls.fd = fd;			/* /sys/class/gpio17/value */
	polls.events = POLLPRI;		/* Exceptions */
	
	do	{
		rc = poll(&polls,1,-1);	/* Block */
		if ( is_signaled )
			return -1;	/* Exit if ^C received */
	} while ( rc < 0 && errno == EINTR );
	
	assert(rc > 0);

	lseek(fd,0,SEEK_SET);
	n = read(fd,buf,sizeof buf);	/* Read value */
	assert(n>0);
	buf[n] = 0;

	rc = sscanf(buf,"%d",&n);
	assert(rc==1);
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
 * Main program :
 */
int
main(int argc,char **argv) {
	int fd, v;

	/*
	 * Get GPIO input pin to use :
	 */
	if ( argc != 2 ) {
usage:		fprintf(stderr,"Usage: %s <gpio_in_pin>\n",argv[0]);
		return 1;
	}	
	if ( sscanf(argv[1],"%d",&gpio_inpin) != 1 )
		goto usage;
	if ( gpio_inpin < 0 || gpio_inpin >= 32 )
		goto usage;

	signal(SIGINT,sigint_handler);		/* Trap on SIGINT */
	fd = gpio_open_edge(gpio_inpin,"both");	/* GPIO input */

	puts("Monitoring for GPIO input changes:\n");

	while ( (v = gpio_poll(fd)) >= 0 ) {	/* Block until input changes */
		printf("GPIO %d changed: %d\n",gpio_inpin,v);
	} while ( !is_signaled );		/* Quit if ^C'd */

	putchar('\n');
	close(fd);				/* Close gpio%d/value */
	gpio_close(gpio_inpin);			/* Unexport gpio */
	return 0;
}

/*********************************************************************
 * End evinput.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
