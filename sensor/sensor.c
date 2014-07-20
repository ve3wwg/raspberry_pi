/*********************************************************************
 * sensor.c - Sense SW1, send to console (and take LED cmd from console)
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <poll.h>
#include <pthread.h>

#include <zmq.h>

static const char *service_sensor_pub = "tcp://*:9999";
static const char *service_sensor_pull = "tcp://*:9998";

static void *context = 0;		/* ZMQ context */
static void *publisher = 0;		/* Publishing socket */
static void *console = 0;		/* Pull socket */

static int SW1 = 0;			/* Switch status */
static int LED = 0;			/* LED status */
static int stop = 0;			/* Non-zero when shutting down */

static int gp_SW1 = 22;			/* GPIO 22 (input) */
static int gp_LED = 27;			/* GPIO 22 (output) */
static int fd_SW1 = -1;			/* Open fd for SW1 */

#include "mutex.c"

/*
 * Publish the LED setting to the console(s)
 */
static void
publish_LED(void) {
	char buf[256];
	size_t n;
	int rc;

	n = sprintf(buf,"led:%d",LED);
	mutex_lock();
	rc = zmq_send(publisher,buf,n,0);
	assert(rc!=-1);
	mutex_unlock();
}

/*
 * Publish the switch setting to the console(s)
 */
static void
publish_SW1(void) {
	char buf[256];
	size_t n;
	int rc;

	n = sprintf(buf,"sw1:%d",SW1);

	mutex_lock();
	rc = zmq_send(publisher,buf,n,0);
	assert(rc!=-1);
	mutex_unlock();
}

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
 * Open /sys/class/gpio%d/value for output :
 */
static int
gpio_open_output(int pin) {
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
	fprintf(f,"out\n");
	fclose(f);

	/* Value :	/sys/class/gpio%d/value */
	gpio_setpath(pin,gp_value,buf,sizeof buf);
	fd = open(buf,O_WRONLY);
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
gpio_poll(int fd) {
	struct pollfd polls;
	char buf[32];
	int rc, n;
	
	polls.fd = fd;			/* /sys/class/gpio17/value */
	polls.events = POLLPRI;		/* Exceptions */
	
	do	{
		rc = poll(&polls,1,-1);	/* Block */
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
 * Write to the GPIO pin
 */
static void
gpio_write(int fd,int dbit) {
	write(fd,dbit?"1\n":"0\n",2);
}

/*
 * Monitor switch changes on GPIO
 */
static void *
SW1_monitor_thread(void *arg) {
	int rc;

	while ( !stop ) {
		rc = gpio_poll(fd_SW1);		/* Watch for SW1 changes */
		if ( rc < 0 )
			break;
		SW1 = rc;
		publish_SW1();
	}
	return 0;
}

/*
 * Periodic broadcast to consoles thread
 */
static void *
console_thread(void *arg) {

	while ( !stop ) {
		sleep(3);
		publish_SW1();
		publish_LED();
	}
	return 0;
}

/*********************************************************************
 * Main thread: read switch changes and publish to console(s)
 *********************************************************************/
int
main(int argc,char **argv) {
	pthread_t tid;
	int rc = 0;
	char buf[256];
	int fd_LED = -1;			/* GPIO 27 */

	mutex_init();

	/* Open GPIO for LED */
	fd_LED = gpio_open_output(gp_LED);
	if ( fd_LED < 0 ) {
		printf("%s: Opening GPIO %d for output.\n",
			strerror(errno),gp_LED);
		return 1;
	}

	/* Open GPIO for SW1 */
	fd_SW1 = gpio_open_edge(22,"both");	/* GPIO input */
	if ( fd_SW1 < 0 ) {
		printf("%s: Opening GPIO %d for input.\n",
			strerror(errno),gp_SW1);
		return 1;
	}

	context = zmq_ctx_new();
	assert(context);

	/* Create a ZMQ publishing socket */
	publisher = zmq_socket(context,ZMQ_PUB);
	assert(publisher);
	rc = zmq_bind(publisher,service_sensor_pub);
	assert(!rc);

	/* Create a console PULL socket */
    	console = zmq_socket(context, ZMQ_PULL);
	assert(console);
        rc = zmq_bind(console,service_sensor_pull);
	assert(rc != -1);

	SW1 = 0;
	publish_SW1();
	publish_LED();

	rc = pthread_create(&tid,0,SW1_monitor_thread,0);
	assert(!rc);

	rc = pthread_create(&tid,0,console_thread,0);
	assert(!rc);

	/*
	 * In this thread, we "pull" console commands :
	 *
	 * led:n	change state of LED
	 * stop:	shutdown the sensor
	 */
	for (;;) {
		rc = zmq_recv(console,buf,sizeof buf-1,0);
		if ( rc > 0 ) {
			buf[rc] = 0;
			if ( !strncmp(buf,"led:",4) ) {
				/* LED command from console */
				buf[rc] = 0;
				sscanf(buf,"led:%d",&LED);
				gpio_write(fd_LED,LED);
				publish_LED();
			}
			if ( !strncmp(buf,"stop:",5) ) {
				stop = 1;
				break;
			}
		}
	}

	mutex_lock();
	zmq_close(console);
	console = 0;

	rc = zmq_send(publisher,"off:",4,0);
	assert(rc!=-1);
	sleep(3);
	zmq_close(publisher);
	publisher = 0;

	gpio_close(gp_SW1);
	gpio_close(gp_LED);
	mutex_unlock();

	return 0;
}

/*********************************************************************
 * End sensor.c - by Warren Gay
 * Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
