/*********************************************************************
 * sysgpio.c : Open/configure /sys GPIO pin
 *
 * Here we must open the /sys/class/gpio/gpio17/value and do a
 * poll(2) on it, so that we can sense the MCP23017 interrupts.
 *********************************************************************/

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
gpio_open_edge(int pin) {
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
	fprintf(f,"falling\n");
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
	unsigned char buf[32];
	struct pollfd polls;
	int rc;
	
	polls.fd = fd;			/* /sys/class/gpio17/value */
	polls.events = POLLPRI;		/* Exceptions */
	
	do	{
		rc = poll(&polls,1,-1);	/* Block */
		if ( is_signaled )
			return -1;	/* Exit if ^C received */
	} while ( rc < 0 && errno == EINTR );
	
	(void)read(fd,buf,sizeof buf);	/* Clear interrupt */
	return 0;
}

/*********************************************************************
 * End sysgpio.c - Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
