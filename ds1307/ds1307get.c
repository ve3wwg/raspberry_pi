/*********************************************************************
 * ds1307get.c : Read real-time DS1307 clock on I2C bus
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "i2c_common.c"			/* I2C routines */
#include "ds1307.h"			/* DS1307 types */

/* Change to i2c-0 if using early Raspberry Pi */
static const char *node = "/dev/i2c-1";

/*
 * Read: [S] 0xB1 <regaddr> <rtcbuf[0]> ... <rtcbuf[n-1]> [P]
 */
static int
i2c_rd_rtc(ds1307_rtc_regs *rtc) {
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg iomsgs[2];
	char zero = 0x00;		/* Register 0x00 */

	iomsgs[0].addr = 0x68;		/* DS1307 */
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = &zero;		/* Register 0x00 */
	iomsgs[0].len = 1;

	iomsgs[1].addr = 0x68;		/* DS1307 */
	iomsgs[1].flags = I2C_M_RD;	/* Read */
	iomsgs[1].buf = (char *)rtc;
	iomsgs[1].len = sizeof *rtc;

	msgset.msgs = iomsgs;
	msgset.nmsgs = 2;

	return ioctl(i2c_fd,I2C_RDWR,&msgset);
}

/*
 * Main program :
 */
int
main(int argc,char **argv) {
	ds1307_rtc_regs rtc;		/* 8 DS1307 Register Values */
	struct tm t0, t1;		/* Unix date/time values */
	char *date_format = getenv("DS1307_FORMAT");
	char dtbuf[256];		/* Formatted date/time */
	int rc;				/* Return code */

	/*
	 * If no environment variable named DS1307_FORMAT, then
	 * set a default date/time format.
	 */
	if ( !date_format )
		date_format = "%Y-%m-%d %H:%M:%S (%A)";

	/*
	 * Initialize I2C and clear rtc and t1 structures:
	 */
	i2c_init(node);				/* Initialize for I2C */
	memset(&rtc,0,sizeof rtc);
	memset(&t1,0,sizeof t1);

	rc = i2c_rd_rtc(&rtc);
	if ( rc < 0 ) {
		perror("Reading DS1307 RTC clock.");
		exit(1);
	} else if ( rc != 2 ) {
		fprintf(stderr,"Read error: got %d of 2 msgs.\n",rc);
		exit(1);
	} else
		rc = 0;

	/*
	 * Check the date returned by the RTC:
	 */
	memset(&t1,0,sizeof t1);
	t1.tm_year = (rtc.year_10s * 10 + rtc.year_1s) + 2000 - 1900;
	t1.tm_mon  = rtc.month_10s * 10 + rtc.month_1s - 1;
	t1.tm_mday = rtc.day_10s   * 10 + rtc.day_1s;
	t1.tm_hour = rtc.hour_10s  * 10 + rtc.hour_1s;
	t1.tm_min  = rtc.mins_10s  * 10 + rtc.mins_1s;
	t1.tm_sec  = rtc.secs_10s  * 10 + rtc.secs_1s;
	t1.tm_isdst = -1;			/* Determine if daylight savings */

	t0 = t1;
	if ( mktime(&t1) == 1L 			/* t1 is modified */
	  || t1.tm_year   != t0.tm_year || t1.tm_mon    != t0.tm_mon
	  || t1.tm_mday   != t0.tm_mday || t1.tm_hour   != t0.tm_hour
	  || t1.tm_min    != t0.tm_min  || t1.tm_sec    != t0.tm_sec ) {
		strftime(dtbuf,sizeof dtbuf,date_format,&t0);
		fprintf(stderr,"Read RTC date is not valid: %s\n",dtbuf);
		exit(2);
	}

	if ( t1.tm_wday != rtc.wkday-1 ) {
		fprintf(stderr,
			"Warning: RTC weekday is incorrect %d but should be %d\n",
			rtc.wkday,t1.tm_wday);
	}

#if 0	/* Change to a 1 for debugging */
	printf("  %d%d-%d%d-%d%d %d%d:%d%d:%d%d (wkday %d)\n",
		rtc.year_10s,rtc.year_1s,
		rtc.month_10s,rtc.month_1s,
		rtc.day_10s,rtc.day_1s,
		rtc.hour_10s,rtc.hour_1s,
		rtc.mins_10s,rtc.mins_1s,
		rtc.secs_10s,rtc.secs_1s,
		rtc.wkday);
#endif
	strftime(dtbuf,sizeof dtbuf,date_format,&t1);
	puts(dtbuf);

	i2c_close();
	return rc == 8 ? 0 : 4;
}

/*********************************************************************
 * End ds1307get.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
