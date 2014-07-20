/*********************************************************************
 * ds1307set.c : Set real-time DS1307 clock on I2C bus
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
 * Write [S] 0xB0 <regaddr> <rtcbuf[0]> ... <rtcbuf[n-1]> [P]
 */
static int
i2c_wr_rtc(ds1307_rtc_regs *rtc) {
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg iomsgs[1];
	char buf[sizeof *rtc + 1];	/* Work buffer */

	buf[0] = 0x00;			/* Register 0x00 */
	memcpy(buf+1,rtc,sizeof *rtc);	/* Copy RTC info */

	iomsgs[0].addr = 0x68;		/* DS1307 Address */
	iomsgs[0].flags = 0;		/* Write */
	iomsgs[0].buf = buf;		/* Register + data */
	iomsgs[0].len = sizeof *rtc + 1; /* Total msg len */

	msgset.msgs = &iomsgs[0];
	msgset.nmsgs = 1;

	return ioctl(i2c_fd,I2C_RDWR,&msgset);
}

/*********************************************************************
 * Set the DS1307 real-time clock on the I2C bus:
 *
 * ./ds1307set YYYYMMDDHHMM[ss]
 *********************************************************************/
int
main(int argc,char **argv) {
	ds1307_rtc_regs rtc;		/* 8 DS1307 Register Values */
	char buf[32];			/* Extraction buffer */
	struct tm t0, t1;		/* Unix date/time values */
	int v, cx, slen;
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
	 * Check command line usage:
	 */
	if ( argc != 2 || (slen = strlen(argv[1])) < 12 || slen > 14 ) {
usage:		fprintf(stderr,
			"Usage: %s YYYYMMDDhhmm[ss]\n",
			argv[0]);
		exit(1);
	}

	/*
	 * Make sure every character is a digit in argument 1.
	 */
	for ( cx=0; cx<slen; ++cx ) 
		if ( !isdigit(argv[1][cx]) )
			goto usage;		/* Not a numeric digit */

	/*
	 * Initialize I2C and clear rtc and t1 structures:
	 */
	i2c_init(node);				/* Initialize for I2C */
	memset(&rtc,0,sizeof rtc);
	memset(&t1,0,sizeof t1);

	/*
	 * Extract YYYYMMDDhhmm[ss] from argument 1:
	 */
	strncpy(buf,argv[1],4)[4] = 0;		/* buf[] = "YYYY" */
	if ( sscanf(buf,"%d",&v) != 1 || v < 2000 || v > 2099 )
		goto usage;
	t1.tm_year = v - 1900;

	strncpy(buf,argv[1]+4,2)[2] = 0;	/* buf[] = "MM" */
	if ( sscanf(buf,"%d",&v) != 1 || v <= 0 || v > 12 )
		goto usage;
	t1.tm_mon = v - 1;			/* 0 - 11 */

	strncpy(buf,argv[1]+6,2)[2] = 0;	/* buf[] = "DD" */
	if ( sscanf(buf,"%d",&v) != 1 || v <= 0 || v > 31 )
		goto usage;
	t1.tm_mday = v;			/* 1 - 31 */
	
	strncpy(buf,argv[1]+8,2)[2] = 0;	/* buf[] = "hh" */
	if ( sscanf(buf,"%d",&v) != 1 || v < 0 || v > 23 )
		goto usage;
	t1.tm_hour = v;

	strncpy(buf,argv[1]+10,2)[2] = 0;	/* buf[] = "mm" */
	if ( sscanf(buf,"%d",&v) != 1 || v < 0 || v > 59 )
		goto usage;
	t1.tm_min = v;

	if ( slen > 12 ) {
		/* Optional ss was provided: */
		strncpy(buf,argv[1]+12,2)[2] = 0; /* buf[] = "ss" */
		if ( sscanf(buf,"%d",&v) != 1 || v < 0 || v > 59 )
			goto usage;
		t1.tm_sec = v;
	}

	/*
	 * Check the validity of the date:
	 */
	t1.tm_isdst = -1;			/* Determine if daylight savings */
	t0 = t1;				/* Save initial values */
	if ( mktime(&t1) == 1L ) {		/* t1 is modified */
bad_date:	printf("Argument '%s' is not a valid calendar date.\n",argv[1]);
		exit(2);
	}

	/*
	 * If struct t1 was adjusted, then the original date/time
	 * values were invalid:
	 */
	if ( t0.tm_year != t1.tm_year || t0.tm_mon  != t1.tm_mon
	  || t0.tm_mday != t1.tm_mday || t0.tm_hour != t1.tm_hour
	  || t0.tm_min  != t1.tm_min  || t0.tm_sec  != t1.tm_sec )
	  	goto bad_date;

	/*
	 * Populate DS1307 registers:
	 */
	rtc.secs_10s = t1.tm_sec / 10;
	rtc.secs_1s  = t1.tm_sec % 10;
	rtc.mins_10s = t1.tm_min / 10;
	rtc.mins_1s  = t1.tm_min % 10;
	rtc.hour_10s = t1.tm_hour / 10;
	rtc.hour_1s  = t1.tm_hour % 10;
	rtc.month_10s= (t1.tm_mon + 1) / 10;
	rtc.month_1s = (t1.tm_mon + 1) % 10;
	rtc.day_10s  = t1.tm_mday / 10;
	rtc.day_1s   = t1.tm_mday % 10;
	rtc.year_10s = (t1.tm_year + 1900 - 2000) / 10;
	rtc.year_1s  = (t1.tm_year + 1900 - 2000) % 10;

	rtc.wkday    = t1.tm_wday + 1;	/* Weekday 1-7 */
	rtc.mode_1224 = 0;		/* Use 24 hour format */

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
	rc = i2c_wr_rtc(&rtc);

	/*
	 * Display RTC values submitted:
	 */
	strftime(dtbuf,sizeof dtbuf,date_format,&t1);
	puts(dtbuf);

	if ( rc < 0 )
		perror("Writing to DS1307 RTC");
	else if ( rc != 1 )
		printf("Incomplete write: %d msgs of 2 written\n",rc);

	i2c_close();
	return rc == 1 ? 0 : 4;
}

/*********************************************************************
 * End ds1307set.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
