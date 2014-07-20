/*********************************************************************
 * mcp23017.c : Interface with MCP23017 I/O Extender Chip
 *
 * This code assumes the following:
 *
 *	1. MCP23017 is configured for address 0x20
 *	2. RPi's GPIO 17 (GEN0) will be used for sensing interrupts
 *	3. Assumed there is a pullup on GPIO 17.
 *	4. MCP23017 GPA4-7 and GPB4-7 will be inputs, with pullups.
 *	5. MCP23017 GPA0-3 and GPB0-3 will be ouputs.
 *	6. MCP23017 signals interrupt active low.
 *	7. MCP23017 operating in non-banked register mode.
 *
 * Inputs sensed will be copied to outputs:
 *	1. GPA4-7 copied to GPA0-3
 *	2. GPB4-7 copied to GPB0-3
 *
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/i2c-dev.h>

#include "i2c_funcs.c"			/* I2C routines */

/* Change to i2c-0 if using early Raspberry Pi */
static const char *node = "/dev/i2c-1";

#define GPIOA	0
#define GPIOB	1

#define IODIR	0
#define IPOL	1
#define GPINTEN	2
#define DEFVAL	3
#define INTCON	4
#define IOCON	5
#define GPPU	6
#define INTF	7
#define INTCAP	8
#define GPIO	9
#define OLAT	10

#define MCP_REGISTER(r,g) (((r)<<1)|(g)) /* For I2C routines */

static unsigned gpio_addr = 0x20;	/* MCP23017 I2C Address */
static const int gpio_inta = 17;	/* GPIO pin for INTA connection */
static int is_signaled = 0;		/* Exit program if signaled */

#include "sysgpio.c"

/*
 * Signal handler to quit the program :
 */
static void
sigint_handler(int signo) {
	is_signaled = 1;		/* Signal to exit program */
}

/*
 * Write to MCP23017 A or B register set:
 */
static int
mcp23017_write(int reg,int AB,int value) {
	unsigned reg_addr = MCP_REGISTER(reg,AB);
	int rc;

	rc = i2c_write8(gpio_addr,reg_addr,value);
	return rc;
}

/*
 * Write value to both MCP23017 register sets :
 */
static void
mcp23017_write_both(int reg,int value) {
	mcp23017_write(reg,GPIOA,value);	/* Set A */
	mcp23017_write(reg,GPIOB,value);	/* Set B */
}

/*
 * Read the MCP23017 input pins (excluding outputs,
 * 16-bits) :
 */
static unsigned
mcp23017_inputs(void) {
	unsigned reg_addr = MCP_REGISTER(GPIO,GPIOA);

	return i2c_read16(gpio_addr,reg_addr) & 0xF0F0;
}

/*
 * Write 16-bits to outputs :
 */
static void
mcp23017_outputs(int value) {
	unsigned reg_addr = MCP_REGISTER(GPIO,GPIOA);

	i2c_write16(gpio_addr,reg_addr,value & 0x0F0F);
}

/*
 * Read MCP23017 captured values (16-bits):
 */
static unsigned
mcp23017_captured(void) {
	unsigned reg_addr = MCP_REGISTER(INTCAP,GPIOA);

	return i2c_read16(gpio_addr,reg_addr) & 0xF0F0;
}

/*
 * Read interrupting input flags (16-bits):
 */
static unsigned
mcp23017_interrupts(void) {
	unsigned reg_addr = MCP_REGISTER(INTF,GPIOA);

	return i2c_read16(gpio_addr,reg_addr) & 0xF0F0;
}

/*
 * Configure the MCP23017 GPIO Extender :
 */
static void
mcp23017_init(void) {
	int v, int_flags;

	mcp23017_write_both(IOCON,0b01000100);	/* MIRROR=1,ODR=1 */
	mcp23017_write_both(GPINTEN,0x00);	/* No interrupts enabled */
	mcp23017_write_both(DEFVAL,0x00);	/* Clear default value */
	mcp23017_write_both(OLAT,0x00);		/* OLATx=0 */
	mcp23017_write_both(GPPU,0b11110000);	/* 4-7 are pullup */
	mcp23017_write_both(IPOL,0b00000000);   /* No inverted polarity */
	mcp23017_write_both(IODIR,0b11110000);	/* 4-7 are inputs, 0-3 outputs */
	mcp23017_write_both(INTCON,0b00000000);	/* Cmp inputs to previous */
	mcp23017_write_both(GPINTEN,0b11110000); /* Interrupt on changes */

	/*
	 * Loop until all interrupts are cleared:
	 */
	do	{
		int_flags = mcp23017_interrupts();
		if ( int_flags != 0 ) {
			v = mcp23017_captured();
			printf("  Got change %04X values %04X\n",int_flags,v);
		}
	} while ( int_flags != 0x0000 && !is_signaled );
}

/*
 * Copy input bit settings to outputs :
 */
static void
post_outputs(void) {
	int inbits = mcp23017_inputs();		/* Read inputs */
	int outbits = inbits >> 4;		/* Shift to output bits */
	mcp23017_outputs(outbits);		/* Copy inputs to outputs */
	printf("  Outputs:      %04X\n",outbits);
}

/*
 * Main program :
 */
int
main(int argc,char **argv) {
	int int_flags, v;
	int fd;

	signal(SIGINT,sigint_handler);		/* Trap on SIGINT */

	i2c_init(node);				/* Initialize for I2C */
	mcp23017_init();			/* Configure MCP23017 @ 20 */

	fd = gpio_open_edge(gpio_inta);		/* Configure INTA pin */

	puts("Monitoring for MCP23017 input changes:\n");
	post_outputs();				/* Copy inputs to outputs */

	do	{
		gpio_poll(fd);			/* Pause until an interrupt */

		int_flags = mcp23017_interrupts();
		if ( int_flags ) {
			v = mcp23017_captured();
			printf("  Input change: flags %04X values %04X\n",
				int_flags,v);
			post_outputs();
		}
	} while ( !is_signaled );		/* Quit if ^C'd */

	fputc('\n',stdout);

	i2c_close();				/* Close I2C driver */
	close(fd);				/* Close gpio17/value */
	gpio_close(gpio_inta);			/* Unexport gpio17 */
	return 0;
}

/*********************************************************************
 * End mcp23017.c - Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
