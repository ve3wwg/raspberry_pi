/*********************************************************************
 * ds1307.h : Common DS1307 types and macro definitions
 *********************************************************************/

typedef struct {
	/* Register Address 0x00 : Seconds */
	unsigned char	secs_1s  : 4;	/* Ones digit: seconds */
	unsigned char	secs_10s : 3;	/* Tens digit: seconds */
	unsigned char	ch       : 1;	/* CH bit */
	/* Register Address 0x01 : Minutes */
	unsigned char	mins_1s  : 4;	/* Ones digit: minutes */
	unsigned char	mins_10s : 3;	/* Tens digit: minutes */
	unsigned char	mbz_1    : 1;	/* Zero bit */
	/* Register Address 0x02 : Hours */
	unsigned char	hour_1s  : 4;	/* Ones digit: hours */
	unsigned char	hour_10s : 2;	/* Tens digit: hours (24hr mode) */
	unsigned char	mode_1224: 1;	/* Mode bit: 12/24 hour format */	
	/* Register Address 0x03 : Weekday */
	unsigned char	wkday    : 3;	/* Day of week (1-7) */
	unsigned char	mbz_2    : 5;	/* Zero bits */
	/* Register Address 0x04 : Day of Month */
	unsigned char	day_1s   : 4;	/* Ones digit: day of month (1-31) */
	unsigned char	day_10s  : 2;	/* Tens digit: day of month */
	unsigned char	mbz_3    : 2;	/* Zero bits */
	/* Register Address 0x05 : Month */
	unsigned char	month_1s : 4;	/* Ones digit: month (1-12) */
	unsigned char	month_10s: 1;	/* Tens digit: month */
	unsigned char	mbz_4    : 3;	/* Zero */
	/* Register Address 0x06 : Year */
	unsigned char	year_1s  : 4;	/* Ones digit: year (00-99) */
	unsigned char	year_10s : 4;	/* Tens digit: year */
	/* Register Address 0x07 : Control */
	unsigned char	rs0      : 1;	/* RS0 */
	unsigned char	rs1      : 1;	/* RS1 */
	unsigned char	mbz_5    : 2;	/* Zeros */
	unsigned char	sqwe     : 1;	/* SQWE */
	unsigned char	mbz_6    : 2;
	unsigned char	outbit	 : 1;	/* OUT */
} ds1307_rtc_regs;

/*********************************************************************
 * End ds1307.h - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
   
