/*********************************************************************
 * softpwm.c - Software PWM example program
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include "gpio_io.c"

typedef struct {
	int		gpio;	/* GPIO output pin */
	double		freq;	/* Operating frequency */
	unsigned	n;	/* The N in N/M */
	unsigned 	m;	/* The M in N/M */
	pthread_t	thread;	/* Controlling thread */
	volatile char	chgf;	/* True when N/M changed */
	volatile char	stopf;	/* True when thread to stop */
} PWM;

/*
 * Timed wait from a float
 */
static void
float_wait(double seconds) {
	fd_set mt;
	struct timeval timeout;
	int rc;

	FD_ZERO(&mt);
	timeout.tv_sec = floor(seconds);
	timeout.tv_usec = floor((seconds - floor(seconds)) * 1000000);

	do  {
		rc = select(0,&mt,&mt,&mt,&timeout);
	} while ( rc < 0 && timeout.tv_sec && timeout.tv_usec );
}

/*
 * Thread performing the PWM function :
 */
static void *
soft_pwm(void *arg) {
	PWM *pwm = (PWM *)arg;
	double fperiod, percent, ontime;

	while ( !pwm->stopf ) {
		fperiod = 1.0 / pwm->freq;
		percent = (double) pwm->n / (double) pwm->m;
		ontime = fperiod * percent;
		for ( pwm->chgf=0; !pwm->chgf && !pwm->stopf; ) {
			gpio_write(pwm->gpio,1);
			float_wait(ontime);

			gpio_write(pwm->gpio,0);
			float_wait(fperiod-ontime);
		}
	}

	return 0;
}

/*
 * Open a soft PWM object:
 */
PWM *
pwm_open(int gpio,double freq) {
	PWM *pwm = malloc(sizeof *pwm);

	pwm->gpio = gpio;
	pwm->freq = freq;
	pwm->thread = 0;
	pwm->n = pwm->m = 0;
	pwm->chgf = 0;
	pwm->stopf = 0;

	INP_GPIO(pwm->gpio);
	OUT_GPIO(pwm->gpio);
	return pwm;
}

/*
 * Close the soft PWM object:
 */
void
pwm_close(PWM *pwm) {
	pwm->stopf = 1;
	if ( pwm->thread )
		pthread_join(pwm->thread,0);
	pwm->thread = 0;
	free(pwm);
}

/*
 * Set PWM Ratio:
 */
void
pwm_ratio(PWM *pwm,unsigned n,unsigned m) {
	pwm->n = n <= m ? n : m;
	pwm->m = m;
	if ( !pwm->thread )
		pthread_create(&pwm->thread,0,soft_pwm,pwm);
	else	pwm->chgf = 1;
}

/*
 * Main program:
 */
int
main(int argc,char **argv) {
	int n, m = 100;
	float f = 1000.0;
	PWM *pwm;
	FILE *pipe;
	char buf[64];
	float pct, total;
    
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

	gpio_init();

	if ( argc > 1 ) {
		/* Run PWM mode */
		pwm = pwm_open(22,1000.0);	/* GPIO 22 (GEN3) */
		pwm_ratio(pwm,n,m);		/* Set ratio n/m */

		printf("PWM set for %d/%d, frequency %.1f (for 60 seconds)\n",n,m,f);

		sleep(60);

		printf("Closing PWM..\n");
		pwm_close(pwm);
	} else	{
		/* Run CPU Meter */
		puts("CPU Meter Mode:");

		pwm = pwm_open(22,500.0);	/* GPIO 22 (GEN3) */
		pwm_ratio(pwm,1,100);		/* Start at 1% */

		for (;;) {
			pipe = popen("ps -eo pcpu|sed 1d","r");
			for ( total=0.0; fgets(buf,sizeof buf,pipe); ) {
				sscanf(buf,"%f",&pct);
				total += pct;
			}
			pclose(pipe);

			pwm_ratio(pwm,total,100);

			printf("\r%.1f%%       ",total);
			fflush(stdout);

			usleep(300000);
		}
	}

	return 0;
}

/*********************************************************************
 * End softpwm.c
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
