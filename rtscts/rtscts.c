/*********************************************************************
 * rtscts.c - Configure GPIO 17 & 30 for RTS & CTS
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <signal.h>

#include "gpio_io.c"			/* GPIO routines */

static inline void
gpio_setalt(int gpio,unsigned alt) {
	INP_GPIO(gpio);
	SET_GPIO_ALT(gpio,alt);
}

int
main(int argc,char **argv) {

	gpio_init();   			/* Initialize GPIO access */
	gpio_setalt(17,3);		/* GPIO 17 ALT = 3 */
	gpio_setalt(30,3);		/* GPIO 30 ALT = 3 */

	return 0;
}

/*********************************************************************
 * End rtscts.c - by Warren Gay
 * Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
