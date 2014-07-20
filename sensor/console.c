/*********************************************************************
 * console.c - Raspberry Pi Sensor Console
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <curses.h>

#include <zmq.h>

#include "mutex.c"

static char *host_name = "localhost";	/* Default host name */
static char service_sensor_pub[128];	/* Service name for sensor */
static char service_sensor_pull[128];	/* Service name for sensor's cmds */

static void *context = 0;		/* ZMQ context object */
static void *subscriber = 0;		/* Subscriber socket */
static void *console = 0;		/* Push socket */

static int SW1 = -1;			/* Known status of SW1 */
static int LED = -1;			/* Known status of LED */

/*
 * Post the status of SW1 to the console screen
 */
static void
post_SW1(void) {

	mutex_lock();			/* Lock for shard curses access */
	attrset(A_REVERSE);
	mvprintw(3,4,"SW1:");
	attrset(A_NORMAL);
	move(3,9);
	if ( SW1 < 0 ) {
		addstr("???");
	} else	{
		if ( SW1 ) {
			attrset(A_BOLD);	/* Blink when switch on */
			addstr("On ");
		} else	{
			attrset(A_NORMAL);
			addstr("Off");		/* SW1 is off */
		}		
	}
	attrset(A_NORMAL);
	if ( SW1 >= 0 || LED >= 0 )
		mvprintw(7,15,"ONLINE ");
	move(7,12);
	refresh();
	mutex_unlock();			/* Done with curses */
}

/*
 * Post LED status to console screen
 */
static void
post_LED(void) {

	mutex_lock();			/* Lock shared curses access */
	attrset(A_REVERSE);
	mvprintw(5,4,"LED:");
	attrset(A_NORMAL);
	move(5,9);

	if ( LED < 0 ) {
		addstr("???");
	} else	{
		if ( LED ) {
			attrset(A_BOLD);
			addstr("On ");	/* LED is now on */
		} else	{
			attrset(A_NORMAL);
			addstr("Off");	/* LED is now off */
		}		
	}
	
	attrset(A_NORMAL);
	if ( SW1 >= 0 || LED >= 0 )
		mvprintw(7,15,"ONLINE ");
	move(7,12);
	refresh();
	mutex_unlock();			/* Release hold on curses */
}

/*
 * Post online status to screen
 */
static void
post_offline(void) {

	SW1 = -1;
	LED = -1;

	mutex_lock();			/* Lock for shard curses access */
	attrset(A_REVERSE|A_BLINK);
	mvprintw(7,15,"OFFLINE");
	refresh();
	mutex_unlock();			/* Done with curses */

	post_LED();
	post_SW1();
}

/*
 * Main console thread for command center
 */
static void *
command_center(void *ignored) {
	int rc;

	post_LED();			/* Post unknown LED status */
	post_SW1();			/* Post unknown SW1 status */

	for (;;) {
		mutex_lock();		/* Lock curses */
		move(7,12);		/* Move cursor to command point */
		refresh();
		mutex_unlock();		/* Release curses */

		rc = getch();		/* Wait for keystroke */

		mutex_lock();		/* Lock curses */
		mvaddch(7,12,rc);	/* Echo character that was typed */
		refresh();
		mutex_unlock();		/* Release curses */

		switch ( rc ) {
		case '0' :
			/* Tell sensor to turn off LED */
			rc = zmq_send(console,"led:0",5,0);
			assert(rc!=-1);
			break;
		case '1' :
			/* Tell sensor to turn on LED */
			rc = zmq_send(console,"led:1",5,0);
			assert(rc!=-1);
			break;
		case 'x' :
		case 'X' :
			rc = zmq_send(console,"stop:",5,0);
			assert(rc!=-1);
			break;
		case 'q' :
		case 'Q' :
			/* Quit the command console */
			sleep(1);
			clear();
			refresh();
			endwin();
			exit(0);
			break;
		default :
			;
		}
	}
}

/*
 * Main thread: init/receive published SW1/LED status updates
 *
 * Specify the IP number or host name of the sensor on the command
 * line as argument one:  $ ./console myrasp
 */
int
main(int argc,char **argv) {
	char buf[1024];
	int rc;
	pthread_t tid;

	if ( argc > 1 )
		host_name = argv[1];
	sprintf(service_sensor_pub,"tcp://%s:9999",host_name);
	sprintf(service_sensor_pull,"tcp://%s:9998",host_name);

	mutex_init();
	context = zmq_ctx_new();
	assert(context);

	subscriber = zmq_socket(context,ZMQ_SUB);
	assert(subscriber);

    	rc = zmq_connect(subscriber,service_sensor_pub);
	if ( rc == -1 ) perror("zmq_connect\n");
        assert (rc!=-1);

	rc = zmq_setsockopt(subscriber,ZMQ_SUBSCRIBE,"sw1:",4);
	assert(rc!=-1);
	rc = zmq_setsockopt(subscriber,ZMQ_SUBSCRIBE,"led:",4);
	assert(rc!=-1);
	rc = zmq_setsockopt(subscriber,ZMQ_SUBSCRIBE,"off:",4);
	assert(rc!=-1);

	console = zmq_socket(context,ZMQ_PUSH);
	assert(console);

	rc = zmq_connect(console,service_sensor_pull);
	assert(!rc);

	initscr();
	cbreak();
	noecho();
	nonl();

	clear();
	box(stdscr,0,0);
	move(1,2);
	printw("Receiving sensor at: %s",service_sensor_pub);

	attrset(A_UNDERLINE);
	mvaddstr(7,2,"Commands:");
	attrset(A_NORMAL);
	mvaddstr( 9,2,"0 - Turn remote LED off.");
	mvaddstr(10,2,"1 - Turn remote LED on.");
	mvaddstr(11,2,"X - Shutdown sensor node.");
	mvaddstr(12,2,"Q - Quit the console.");
	mvprintw(7,15,"Online?");

	rc = pthread_create(&tid,0,command_center,0);
	assert(!rc);

	for (;;) {
		rc = zmq_recv(subscriber,buf,sizeof buf-1,0);
		assert(rc >= 0 && rc < sizeof buf-1);
		buf[rc] = 0;

		if ( !strncmp(buf,"off:",4) )
			post_offline();

		if ( !strncmp(buf,"sw1:",4) ) {
			sscanf(buf,"sw1:%d",&SW1);
			post_SW1();
		}

		if ( !strncmp(buf,"led:",4) ) {
			sscanf(buf,"led:%d",&LED);
			post_LED();
		}
	}

	return 0;
}

/*********************************************************************
 * End console.c - by Warren Gay
 * Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
