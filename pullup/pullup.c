/*********************************************************************
 * pullup.c : Change the pullup resistor setting for GPIO pin
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <signal.h>

#include "gpio_io.c"                    /* GPIO routines */
#include "timed_wait.c"                 /* Delay */

/*********************************************************************
 * 0x7E200094   GPPUD           GPIO Pin Pull-up/down Enable
 * 0x7E200098   GPPUDCLK0       GPIO Pin Pull-up/down Enable Clock 0
 *********************************************************************/

#define GPIO_GPPUD      *(ugpio+37)
#define GPIO_GPPUDCLK0  *(ugpio+38)

static inline void
gpio_setpullup(int gpio,int pull) {
        unsigned mask = 1 << gpio;      /* GPIOs 0 to 31 only */
        unsigned pmask = pull >= 0 ? ( 1 << !!pull ) : 0;

        GPIO_GPPUD = pmask;             /* Select pullup setting */
        timed_wait(0,500,0);
        GPIO_GPPUDCLK0 = mask;          /* Set the GPIO of interest */
        timed_wait(0,500,0);
        GPIO_GPPUD = 0;                 /* Reset pmask */
        timed_wait(0,500,0);
        GPIO_GPPUDCLK0 = 0;             /* Set the GPIO of interest */
        timed_wait(0,500,0);
}

/*********************************************************************
 * Command line arguments are of the form <gpio>={low,high or none},
 * for example: ./pullup 7=high 8=low
 *
 * Only the first character of the argument after '=' is checked.
 *********************************************************************/
int
main(int argc,char **argv) {
        int x, gpio, p;
        char arg[64];

        gpio_init();

        for ( x=1; x<argc; ++x ) {
                if ( sscanf(argv[x],"%d=%s",&gpio,arg) != 2 )
                        goto errxit;
                if ( *arg == 'n' )
                        p = -1;
                else if ( *arg == 'l' || *arg == 'h' )
                        p = *arg == 'h' ? 1 : 0;
                else    goto errxit;
                if ( gpio < 0 || gpio > 31 ) {
                        fprintf(stderr,"%s: GPIO must be <= 31\n",
                                argv[x]);
                        return 1;
                }
                gpio_setpullup(gpio,p);
        }
        return 0;

errxit: fprintf(stderr,"Argument '%s' must be in the form\n"
                "  <gpio>=<arg> where arg is h, l or n.\n",
                argv[x]);
        return 1;
}

/*********************************************************************
 * End pullup.c - by Warren Gay
 * Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
