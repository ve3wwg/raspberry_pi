/*********************************************************************
 * i2c_common.c : Common I2C Access Functions
 *********************************************************************/

static int i2c_fd = -1;			/* Device node: /dev/i2c-1 */
static unsigned long i2c_funcs = 0;	/* Support flags */

/*
 * Open I2C bus and check capabilities :
 */
static void
i2c_init(const char *node) {
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
 * Close the I2C driver :
 */
static void
i2c_close(void) {
	close(i2c_fd);
	i2c_fd = -1;
}

/* End i2c_common.c */
