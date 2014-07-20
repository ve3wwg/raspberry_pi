/*********************************************************************
 * nunchuk.c : Read events from nunchuck and stuff as mouse events
 *********************************************************************/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include "timed_wait.c"

static int is_signaled = 0;	/* Exit program if signaled */
static int i2c_fd = -1;		/* Open /dev/i2c-1 device */
static int f_debug = 0;		/* True to print debug messages */

typedef struct {
	unsigned char	stick_x;	/* Joystick X */
	unsigned char	stick_y;	/* Joystick Y */
	unsigned	accel_x;	/* Accel X */
	unsigned	accel_y;	/* Accel Y */
	unsigned	accel_z;	/* Accel Z */
	unsigned	z_button : 1;	/* Z button */
	unsigned	c_button : 1;	/* C button */
	unsigned char	raw[6];		/* Raw received data */
} nunchuk_t;

/*
 * Open I2C bus and check capabilities :
 */
static void
i2c_init(const char *node) {
	unsigned long i2c_funcs = 0;	/* Support flags */
	int rc;

	i2c_fd = open(node,O_RDWR);	/* Open driver /dev/i2s-1 */
	if ( i2c_fd < 0 ) {
		perror("Opening /dev/i2s-1");
		puts("Check that the i2c-dev & i2c-bcm2708 kernel modules "
		     "are loaded.");
		abort();
	}

	/*
	 * Make sure the driver supports plain I2C I/O:
	 */
	rc = ioctl(i2c_fd,I2C_FUNCS,&i2c_funcs);
	assert(rc >= 0);
	assert(i2c_funcs & I2C_FUNC_I2C);
}

/*
 * Configure the nunchuk for no encryption:
 */
static void
nunchuk_init(void) {
	static char init_msg1[] = { 0xF0, 0x55 };
	static char init_msg2[] = { 0xFB, 0x00 };
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg iomsgs[1];
	int rc;

	iomsgs[0].addr = 0x52;		/* Address of Nunchuk */
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = init_msg1;	/* Nunchuk 2 byte sequence */
	iomsgs[0].len = 2;		/* 2 bytes */

	msgset.msgs = iomsgs;
	msgset.nmsgs = 1;

	rc = ioctl(i2c_fd,I2C_RDWR,&msgset);
	assert(rc == 1);

	timed_wait(0,200,0);		/* Nunchuk needs time */

	iomsgs[0].addr = 0x52;		/* Address of Nunchuk */
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = init_msg2;	/* Nunchuk 2 byte sequence */
	iomsgs[0].len = 2;		/* 2 bytes */

	msgset.msgs = iomsgs;
	msgset.nmsgs = 1;

	rc = ioctl(i2c_fd,I2C_RDWR,&msgset);
	assert(rc == 1);
}

/*
 * Read nunchuk data:
 */
static int
nunchuk_read(nunchuk_t *data) {
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg iomsgs[1];
	char zero[1] = { 0x00 };	/* Written byte */
	unsigned t;
	int rc;

	timed_wait(0,15000,0);

	/*
	 * Write the nunchuk register address of 0x00 :
	 */
	iomsgs[0].addr = 0x52;		/* Nunchuk address */
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = zero;		/* Sending buf */
	iomsgs[0].len = 1;		/* 6 bytes */

	msgset.msgs = iomsgs;
	msgset.nmsgs = 1;

	rc = ioctl(i2c_fd,I2C_RDWR,&msgset);
	if ( rc < 0 )
		return -1;		/* I/O error */

	timed_wait(0,200,0);		/* Zzzz, nunchuk needs time */

	/*
	 * Read 6 bytes starting at 0x00 :
	 */
	iomsgs[0].addr = 0x52;			/* Nunchuk address */
	iomsgs[0].flags = I2C_M_RD;		/* Read */
	iomsgs[0].buf = (char *)data->raw;	/* Receive raw bytes here */
	iomsgs[0].len = 6;			/* 6 bytes */

	msgset.msgs = iomsgs;
	msgset.nmsgs = 1;

	rc = ioctl(i2c_fd,I2C_RDWR,&msgset);
	if ( rc < 0 )
		return -1;			/* Failed */

	data->stick_x = data->raw[0];
	data->stick_y = data->raw[1];
	data->accel_x = data->raw[2] << 2;
	data->accel_y = data->raw[3] << 2;
	data->accel_z = data->raw[4] << 2;

	t = data->raw[5];
	data->z_button = t & 1 ? 0 : 1;
	data->c_button = t & 2 ? 0 : 1;
	t >>= 2;
	data->accel_x |= t & 3;
	t >>= 2;
	data->accel_y |= t & 3;
	t >>= 2;
	data->accel_z |= t & 3;
	return 0;
}

/*
 * Dump the nunchuk data:
 */
static void
dump_data(nunchuk_t *data) {
	int x;

	printf("Raw nunchuk data: ");
	for ( x=0; x<6; ++x )
		printf(" [%02X]",data->raw[x]);
	putchar('\n');

	printf(".stick_x = %04X ( %4u )\n",data->stick_x,data->stick_x);
	printf(".stick_y = %04X ( %4u )\n",data->stick_y,data->stick_y);
	printf(".accel_x = %04X ( %4u )\n",data->accel_x,data->accel_x);
	printf(".accel_y = %04X ( %4u )\n",data->accel_y,data->accel_y);
	printf(".accel_z = %04X ( %4u )\n",data->accel_z,data->accel_z);
	printf(".z_button= %d\n",data->z_button);
	printf(".c_button= %d\n\n",data->c_button);
}

/*
 * Close the I2C driver :
 */
static void
i2c_close(void) {
	close(i2c_fd);
	i2c_fd = -1;
}

/*
 * Open a uinput node:
 */
static int
uinput_open(void) {
	int fd;
	struct uinput_user_dev uinp;
	int rc;

	fd = open("/dev/uinput",O_WRONLY|O_NONBLOCK);
	if ( fd < 0 ) {
		perror("Opening /dev/uinput");
		exit(1);
	}

	rc = ioctl(fd,UI_SET_EVBIT,EV_KEY);
	assert(!rc);
	rc = ioctl(fd,UI_SET_EVBIT,EV_REL);
	assert(!rc);

	rc = ioctl(fd,UI_SET_RELBIT,REL_X);
	assert(!rc);
	rc = ioctl(fd,UI_SET_RELBIT,REL_Y);
	assert(!rc);

	rc = ioctl(fd,UI_SET_KEYBIT,KEY_ESC);
	assert(!rc);

	ioctl(fd,UI_SET_KEYBIT,BTN_MOUSE);
	ioctl(fd,UI_SET_KEYBIT,BTN_TOUCH);
	ioctl(fd,UI_SET_KEYBIT,BTN_MOUSE);
	ioctl(fd,UI_SET_KEYBIT,BTN_LEFT);
	ioctl(fd,UI_SET_KEYBIT,BTN_MIDDLE);
	ioctl(fd,UI_SET_KEYBIT,BTN_RIGHT);

	memset(&uinp,0,sizeof uinp);
	strncpy(uinp.name,"nunchuk",UINPUT_MAX_NAME_SIZE);
	uinp.id.bustype = BUS_USB;
	uinp.id.vendor  = 0x1;
	uinp.id.product = 0x1;
	uinp.id.version = 1;

	rc = write(fd,&uinp,sizeof(uinp));
	assert(rc == sizeof(uinp));

	rc = ioctl(fd,UI_DEV_CREATE);
	assert(!rc);
	return fd;
}

/*
 * Post keystroke down and keystroke up events:
 * (unused here but available for your own experiments)
 */
static void
uinput_postkey(int fd,unsigned key) {
	struct input_event ev;
	int rc;

	memset(&ev,0,sizeof(ev));
	ev.type = EV_KEY;
	ev.code = key;
	ev.value = 1;			/* Key down */

	rc = write(fd,&ev,sizeof(ev));
	assert(rc == sizeof(ev));

	ev.value = 0;			/* Key up */
	rc = write(fd,&ev,sizeof(ev));
	assert(rc == sizeof(ev));
}	

/*
 * Post a synchronization point :
 */
static void
uinput_syn(int fd) {
	struct input_event ev;
	int rc;

	memset(&ev,0,sizeof(ev));
	ev.type = EV_SYN;
	ev.code = SYN_REPORT;
	ev.value = 0;
	rc = write(fd,&ev,sizeof(ev));
	assert(rc == sizeof(ev));
}

/*
 * Synthesize a button click:
 *	up_down		1=up, 0=down
 *	buttons		1=Left, 2=Middle, 4=Right
 */
static void
uinput_click(int fd,int up_down,int buttons) {
	static unsigned codes[] = { BTN_LEFT, BTN_MIDDLE, BTN_RIGHT };
	struct input_event ev;
	int x;

	memset(&ev,0,sizeof(ev));

	/*
	 * Button down or up events :
	 */
	for ( x=0; x < 3; ++x ) {
		ev.type = EV_KEY;
		ev.value = up_down;		/* Button Up or down */
		if ( buttons & (1 << x) ) {	/* Button 0, 1 or 2 */
			ev.code = codes[x];
			write(fd,&ev,sizeof(ev));
		}
	}
}

/*
 * Synthesize relative mouse movement :
 */
static void
uinput_movement(int fd,int x,int y) {
	struct input_event ev;
	int rc;

	memset(&ev,0,sizeof(ev));
	ev.type = EV_REL;
	ev.code = REL_X;
	ev.value = x;

	rc = write(fd,&ev,sizeof(ev));
	assert(rc == sizeof(ev));

	ev.code = REL_Y;
	ev.value = y;
	rc = write(fd,&ev,sizeof(ev));
	assert(rc == sizeof(ev));
}	

/*
 * Close uinput device :
 */
static void
uinput_close(int fd) {
	int rc;

	rc = ioctl(fd,UI_DEV_DESTROY);
	assert(!rc);
	close(fd);
}

/*
 * Signal handler to quit the program :
 */
static void
sigint_handler(int signo) {
	is_signaled = 1;		/* Signal to exit program */
}

/*
 * Curve the adjustment :
 */
static int
curve(int relxy) {
	int ax = abs(relxy);		/* abs(relxy) */
	int sgn = relxy < 0 ? -1 : 1;	/* sign(relxy) */
	int mv = 1;			/* Smallest step */

	if ( ax > 100 )
		mv = 10;		/* Take large steps */
	else if ( ax > 65 )
		mv = 7;
	else if ( ax > 35 )
		mv = 5;
	else if ( ax > 15 )
		mv = 2;			/* 2nd smallest step */
	return mv * sgn;
}

/*
 * Main program :
 */
int
main(int argc,char **argv) {
	int fd, need_sync, init = 3;
	int rel_x=0, rel_y = 0;
	nunchuk_t data0, data, last;

	if ( argc > 1 && !strcmp(argv[1],"-d") )
		f_debug = 1;			/* Enable debug messages */

	(void)uinput_postkey;			/* Suppress compiler warning about unused */

	i2c_init("/dev/i2c-1");			/* Open I2C controller */
	nunchuk_init();				/* Turn off encryption */

	signal(SIGINT,sigint_handler);		/* Trap on SIGINT */
	fd = uinput_open();			/* Open /dev/uinput */

	while ( !is_signaled ) {
		if ( nunchuk_read(&data) < 0 )
			continue;
		
		if ( f_debug )
			dump_data(&data);	/* Dump nunchuk data */

		if ( init > 0 && !data0.stick_x && !data0.stick_y ) {
			data0 = data;		/* Save initial values */
			last = data;
			--init;
			continue;	
		}

		need_sync = 0;
		if ( abs(data.stick_x - data0.stick_x) > 2 
		  || abs(data.stick_y - data0.stick_y) > 2 ) {
			rel_x = curve(data.stick_x - data0.stick_x);
			rel_y = curve(data.stick_y - data0.stick_y);
			if ( rel_x || rel_y ) {
				uinput_movement(fd,rel_x,-rel_y);
				need_sync = 1;
			}
		}

		if ( last.z_button != data.z_button ) {
			uinput_click(fd,data.z_button,1);
			need_sync = 1;
		}

		if ( last.c_button != data.c_button ) {
			uinput_click(fd,data.c_button,4);
			need_sync = 1;
		}

		if ( need_sync )
			uinput_syn(fd);
		last = data;
	}

	putchar('\n');
	uinput_close(fd);
	i2c_close();
	return 0;
}

/*********************************************************************
 * End nunchuk.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
