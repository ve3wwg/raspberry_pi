/*********************************************************************
 * i2c_funcs.c : I2C Access Functions
 *********************************************************************/

static int i2c_fd = -1;			/* Device node: /dev/i2c-1 */
static unsigned long i2c_funcs = 0;	/* Support flags */

/*
 * Write 8 bits to I2C bus peripheral:
 */
int
i2c_write8(int addr,int reg,int byte) {
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg iomsgs[1];
	char buf[2];
	int rc;

	buf[0] = (unsigned char) reg;	/* MCP23017 register no. */
	buf[1] = (unsigned char) byte;	/* Byte to write to register */

	iomsgs[0].addr = (unsigned) addr;
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = buf;
	iomsgs[0].len = 2;

	msgset.msgs = iomsgs;
	msgset.nmsgs = 1;

	rc = ioctl(i2c_fd,I2C_RDWR,&msgset);
	return rc < 0 ? -1 : 0;
}

/*
 * Write 16 bits to Peripheral at address :
 */
int
i2c_write16(int addr,int reg,int value) {
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg iomsgs[1];
	char buf[3];
	int rc;

	buf[0] = (char) reg;
	buf[1] = (char) (( value >> 8 ) & 0xFF);
	buf[2] = (char) (value & 0xFF);

	iomsgs[0].addr = (unsigned) addr;
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = buf;
	iomsgs[0].len = 3;

	msgset.msgs = iomsgs;
	msgset.nmsgs = 1;

	rc = ioctl(i2c_fd,I2C_RDWR,&msgset);
	return rc < 0 ? -1 : 0;
}

/*
 * Read 8-bit value from peripheral at addr :
 */
int
i2c_read8(int addr,int reg) {
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg iomsgs[2];
	char buf[1], rbuf[1];
	int rc;

	buf[0] = (char) reg;

	iomsgs[0].addr = iomsgs[1].addr = (unsigned) addr;
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = buf;
	iomsgs[0].len = 1;

	iomsgs[1].flags = I2C_M_RD;	/* Read */
	iomsgs[1].buf = rbuf;
	iomsgs[1].len = 1;

	msgset.msgs = iomsgs;
	msgset.nmsgs = 2;

	rc = ioctl(i2c_fd,I2C_RDWR,&msgset);
	return rc < 0 ? -1 : ((int)(rbuf[0]) & 0x0FF);
}

/*
 * Read 16-bits of data from peripheral :
 */
int
i2c_read16(int addr,int reg) {
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg iomsgs[2];
	char buf[1], rbuf[2];
	int rc;

	buf[0] = (char) reg;

	iomsgs[0].addr = iomsgs[1].addr = (unsigned) addr;
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = buf;
	iomsgs[0].len = 1;

	iomsgs[1].flags = I2C_M_RD;
	iomsgs[1].buf = rbuf;		/* Read */
	iomsgs[1].len = 2;

	msgset.msgs = iomsgs;
	msgset.nmsgs = 2;

	if ( (rc = ioctl(i2c_fd,I2C_RDWR,&msgset)) < 0 )
		return -1;
	return (rbuf[0] << 8) | rbuf[1];
}

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

/*********************************************************************
 * End i2c_funcs.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
   
