/*********************************************************************
 * pwm.c - PWM example program
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define BCM2835_PWM_CONTROL 0
#define BCM2835_PWM_STATUS  1
#define BCM2835_PWM0_RANGE  4
#define BCM2835_PWM0_DATA   5

#define BCM2708_PERI_BASE       0x20000000
#define BLOCK_SIZE 		(4*1024)

#define GPIO_BASE               (BCM2708_PERI_BASE + 0x200000)
#define PWM_BASE		(BCM2708_PERI_BASE + 0x20C000)
#define CLK_BASE		(BCM2708_PERI_BASE + 0x101000)

#define	PWMCLK_CNTL 40
#define	PWMCLK_DIV  41

static volatile unsigned *ugpio = 0;
static volatile unsigned *ugpwm = 0;
static volatile unsigned *ugclk = 0;

static struct S_PWM_CTL {
	unsigned	PWEN1 : 1;
	unsigned	MODE1 : 1;
	unsigned	RPTL1 : 1;
	unsigned	SBIT1 : 1;
	unsigned	POLA1 : 1;
	unsigned	USEF1 : 1;
	unsigned	CLRF1 : 1;
	unsigned	MSEN1 : 1;
} volatile *pwm_ctl = 0;

static struct S_PWM_STA {
	unsigned	FULL1 : 1;
	unsigned	EMPT1 : 1;
	unsigned	WERR1 : 1;
	unsigned	RERR1 : 1;
	unsigned	GAP01 : 1;
	unsigned	GAP02 : 1;
	unsigned	GAP03 : 1;
	unsigned	GAP04 : 1;
	unsigned	BERR : 1;
	unsigned	STA1 : 1;
} volatile *pwm_sta = 0;

static volatile unsigned *pwm_rng1 = 0;
static volatile unsigned *pwm_dat1 = 0;

#define INP_GPIO(g) *(ugpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) \
    *(ugpio+(((g)/10))) |= (((a)<=3?(a)+4:((a)==4?3:2))<<(((g)%10)*3))

/*
 * Establish the PWM frequency:
 */
static int
pwm_frequency(float freq) {
	const double clock_rate = 19200000.0;
	long idiv;
	int rc = 0;

	/*
	 * Kill the clock:
	 */
	ugclk[PWMCLK_CNTL] = 0x5A000020;	/* Kill clock */
	pwm_ctl->PWEN1 = 0;     		/* Disable PWM */
	usleep(10);  

	/*
	 * Compute and set the divisor :
	 */
	idiv = (long) ( clock_rate / (double) freq );
	if ( idiv < 1 ) {
		idiv = 1;			/* Lowest divisor */
		rc = -1;
	} else if ( idiv >= 0x1000 ) {
		idiv = 0xFFF;			/* Highest divisor */
		rc = +1;
	}

	ugclk[PWMCLK_DIV] = 0x5A000000 | ( idiv << 12 );
    
	/*
	 * Set source to oscillator and enable clock:
	 */
    	ugclk[PWMCLK_CNTL] = 0x5A000011;

	/*
 	 * GPIO 18 is PWM, when set to Alt Func 5 :
	 */
	INP_GPIO(18);		/* Set ALT = 0 */
	SET_GPIO_ALT(18,5);	/* Or in '5' */

	pwm_ctl->MODE1 = 0;     /* PWM mode */
	pwm_ctl->RPTL1 = 0;
	pwm_ctl->SBIT1 = 0;
	pwm_ctl->POLA1 = 0;
	pwm_ctl->USEF1 = 0;
	pwm_ctl->MSEN1 = 0;     /* PWM mode */
	pwm_ctl->CLRF1 = 1;
	return rc;
}

/*
 * Initialize GPIO/PWM/CLK Access
 */
static void
pwm_init() {
	int fd;	
	char *map;

	fd = open("/dev/mem",O_RDWR|O_SYNC);  /* Needs root access */
	if ( fd < 0 ) {
		perror("Opening /dev/mem");
		exit(1);
	}

	map = (char *) mmap(
		NULL,             /* Any address */
		BLOCK_SIZE,       /* # of bytes */
		PROT_READ|PROT_WRITE,
		MAP_SHARED,       /* Shared */
		fd,               /* /dev/mem */
		PWM_BASE          /* Offset to GPIO */
	);

	if ( (long)map == -1L ) {
		perror("mmap(/dev/mem)");    
		exit(1);
	}

	/* Access to PWM */
	ugpwm = (volatile unsigned *)map;
	pwm_ctl  = (struct S_PWM_CTL *) &ugpwm[BCM2835_PWM_CONTROL];
	pwm_sta  = (struct S_PWM_STA *) &ugpwm[BCM2835_PWM_STATUS];
	pwm_rng1 = &ugpwm[BCM2835_PWM0_RANGE];
	pwm_dat1 = &ugpwm[BCM2835_PWM0_DATA];

	map = (char *) mmap(
		NULL,             /* Any address */
		BLOCK_SIZE,       /* # of bytes */
		PROT_READ|PROT_WRITE,
		MAP_SHARED,       /* Shared */
		fd,               /* /dev/mem */
		CLK_BASE          /* Offset to GPIO */
	);

	if ( (long)map == -1L ) {
		perror("mmap(/dev/mem)");    
		exit(1);
	}

	/* Access to CLK */
	ugclk = (volatile unsigned *)map;

	map = (char *) mmap(
		NULL,             /* Any address */
		BLOCK_SIZE,       /* # of bytes */
		PROT_READ|PROT_WRITE,
		MAP_SHARED,       /* Shared */
		fd,               /* /dev/mem */
		GPIO_BASE         /* Offset to GPIO */
	);

	if ( (long)map == -1L ) {
		perror("mmap(/dev/mem)");    
		exit(1);
	}
	
	/* Access to GPIO */
	ugpio = (volatile unsigned *)map;

	close(fd);
}

/*
 * Set PWM to ratio N/M, and enable it:
 */
static void
pwm_ratio(unsigned n,unsigned m) {

	pwm_ctl->PWEN1 = 0;     /* Disable */

	*pwm_rng1 = m;
	*pwm_dat1 = n;

	if ( !pwm_sta->STA1 ) {
		if ( pwm_sta->RERR1 )
			pwm_sta->RERR1 = 1;
		if ( pwm_sta->WERR1 )
			pwm_sta->WERR1 = 1;
		if ( pwm_sta->BERR )
			pwm_sta->BERR = 1;
	}

	usleep(10);		/* Pause */
	pwm_ctl->PWEN1 = 1;     /* Enable */
}

/*
 * Main program:
 */
int
main(int argc,char **argv) {
	FILE *pipe;
	char buf[64];
	float pct, total;
	int n, m = 100;
	float f = 1000.0;
    
	if ( argc > 1 )
		n = atoi(argv[1]);
	if ( argc > 2 )
		m = atoi(argv[2]);
	if ( argc > 3 )
		f = atof(argv[3]);
	if ( argc > 1 ) {
		if ( n > m || n < 1 || m < 1 || f < 586.0 || f > 19200000.0 ) {
			fprintf(stderr,"Value error: N=%d, M=%d, F=%.1f\n",n,m,f);
			return 1;
		}
	}

	pwm_init();

	if ( argc > 1 ) {
		/* Start PWM */
		pwm_frequency(f);
		pwm_ratio(n,m);
		printf("PWM set for %d/%d, frequency %.1f\n",n,m,f);
	} else	{
		/* Run CPU Meter */
		puts("CPU Meter Mode:");
		for (;;) {
			pipe = popen("ps -eo pcpu|sed 1d","r");
			for ( total=0.0; fgets(buf,sizeof buf,pipe); ) {
				sscanf(buf,"%f",&pct);
				total += pct;
			}
			pclose(pipe);
			printf("\r%.1f%%       ",total);
			fflush(stdout);
			pwm_ratio(total,100);
			usleep(300000);
		}
	}

	return 0;
}

/*********************************************************************
 * End pwm.c
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
