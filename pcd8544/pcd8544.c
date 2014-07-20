/*********************************************************************
 * pcd8544 : Driver for the PCD8544 LCD chip
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <termio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <assert.h>

#include "gpio_io.c"			/* GPIO routines */

/*
 * GPIO definitions :
 */
static const int lcd_ce 	= 25;	/* LCD Chip Enable GPIO */
static const int lcd_res	= 23;	/* LCD /Reset GPIO */
static const int lcd_d_c	= 17;	/* LCD Data/Command GPIO */
static const int lcd_sdin	= 27;	/* LCD Serial Data In GPIO */
static const int lcd_sclk	= 22;	/* LCD Serial Clock GPIO */

/*
 * Application Routines:
 */
const int lcd_lines	= 6;		/* # of Lines for LCD (y) */
const int lcd_cols	= 14;		/* # of Columns for LCD (x) */

void lcd_init(int vop);			/* Initialize PCD8544 */
void lcd_home(void);			/* Home cursor */
void lcd_sety(int y);			/* Move to line 0-5 */
void lcd_setx(int x);			/* Move to col 0-13 */
void lcd_move(int y,int x);		/* Move to y,x */
void lcd_clear(void);			/* Clear screen & home cursor */
void lcd_clrtobot(void);		/* Clear from position to end screen */
void lcd_clrtoeol(void);		/* Clear from position to end of line */
void lcd_printf(const char *format,...);/* Format and display message */

int lcd_getx();				/* Return current X position */
int lcd_gety();				/* Return current Y position */
char lcd_char();			/* Return current char at position */

/*********************************************************************
 * Internal LCD support :
 *********************************************************************/
typedef enum {
	LCD_Unselect,			/* Unselect device */
	LCD_Command,			/* Select for command mode */
	LCD_Data			/* Select for data mode */
} Enable;

static void lcd(Enable en);		/* Enable command/data or unselect */
static void lcd_wr_bit(int b);		/* Write one bit to LCD */
static void lcd_wr_byte(int byte);	/* Write one byte to LCD */
static void lcd_putraw(char c);		/* Internal put text byte */

typedef unsigned char uchr_t;

static int lcd_vop		= 0xBF;	/* Current Vop LCD value */
static int lcd_y		= 0;	/* Current LCD y */
static int lcd_x		= 0;	/* Current LCD x */
static char lcd_buf[6][14];		/* Text buffer for scrolling */

/*********************************************************************
 * Use http://www.carlos-rodrigues.com/projects/pcd8544
 * to edit font.
 *********************************************************************/
static const uchr_t ascii_font[] = {
	0x00, 0x00, 0x00, 0x00,	0x00,	/* Space */
	0x00, 0x00, 0x2f, 0x00, 0x00, 	/* ! */
	0x00, 0x07, 0x00, 0x07, 0x00,	/* " */
	0x0a, 0x1f, 0x0a, 0x1f, 0x0a,	/* # */
	0x12, 0x15, 0x3f, 0x15, 0x09,	/* $ */
	0x13, 0x0b, 0x04, 0x1a, 0x19,	/* % */
	0x0c, 0x12, 0x17, 0x09, 0x10,	/* & */
	0x00, 0x00, 0x07, 0x00, 0x00,	/* ' */
	0x00, 0x0c, 0x12, 0x21, 0x00,	/* ( */
	0x00, 0x21, 0x12, 0x0c, 0x00,	/* ) */
	0x12, 0x0c, 0x1e, 0x0c, 0x12,	/* * */
	0x04, 0x04, 0x1f, 0x04, 0x04,	/* + */
	0x00, 0x40, 0x30, 0x00, 0x00,	/* , */
	0x04, 0x04, 0x04, 0x04, 0x04,	/* - */
	0x00, 0x00, 0x10, 0x00, 0x00,	/* . */
	0x10, 0x08, 0x04, 0x02, 0x01,	/* / */
	0x0e, 0x19, 0x15, 0x13, 0x0e,	/* 0 */
	0x00, 0x12, 0x1f, 0x10, 0x00,	/* 1 */
	0x12, 0x19, 0x15, 0x15, 0x12,	/* 2 */
	0x09, 0x11, 0x15, 0x15, 0x0b,	/* 3 */
	0x0c, 0x0a, 0x09, 0x1f, 0x08,	/* 4 */
	0x17, 0x15, 0x15, 0x15, 0x08,	/* 5 */
	0x0e, 0x15, 0x15, 0x15, 0x08,	/* 6 */
	0x11, 0x09, 0x05, 0x03, 0x01,	/* 7 */
	0x0a, 0x15, 0x15, 0x15, 0x0a,	/* 8 */
	0x02, 0x15, 0x15, 0x15, 0x0e,	/* 9 */
	0x00, 0x00, 0x14, 0x00, 0x00,	/* : */
	0x00, 0x20, 0x14, 0x00, 0x00,	/* ; */
	0x00, 0x04, 0x0a, 0x11, 0x00,	/* < */
	0x00, 0x0a, 0x0a, 0x0a, 0x00,	/* = */
	0x00, 0x11, 0x0a, 0x04, 0x00,	/* > */
	0x02, 0x01, 0x59, 0x09, 0x06,	/* ? */

	0x3c, 0x42, 0x5a, 0x56, 0x1c,	/* @ */
	0x1e, 0x05, 0x05, 0x05, 0x1e,	/* A */
	0x1f, 0x15, 0x15, 0x15, 0x0a,	/* B */
	0x0e, 0x11, 0x11, 0x11, 0x0a,	/* C */
	0x1f, 0x11, 0x11, 0x11, 0x0e,	/* D */
	0x1f, 0x15, 0x15, 0x15, 0x11,	/* E */
	0x1f, 0x05, 0x05, 0x05, 0x01,	/* F */
	0x0e, 0x11, 0x15, 0x15, 0x1c,	/* G */
	0x1f, 0x04, 0x04, 0x04, 0x1f,	/* H */
	0x00, 0x11, 0x1f, 0x11, 0x00,	/* I */
	0x08, 0x10, 0x10, 0x0f, 0x00,	/* J */
	0x1f, 0x04, 0x0a, 0x11, 0x00,	/* K */
	0x1f, 0x10, 0x10, 0x10, 0x10,	/* L */
	0x1f, 0x02, 0x0c, 0x02, 0x1f,	/* M */
	0x1f, 0x02, 0x04, 0x08, 0x1f,	/* N */
	0x0e, 0x11, 0x11, 0x11, 0x0e,	/* O */
	0x1f, 0x05, 0x05, 0x05, 0x02,	/* P */
	0x0e, 0x11, 0x11, 0x19, 0x2e,	/* Q */
	0x1f, 0x05, 0x05, 0x05, 0x1a,	/* R */
	0x06, 0x15, 0x15, 0x15, 0x08,	/* S */
	0x01, 0x01, 0x1f, 0x01, 0x01,	/* T */
	0x0f, 0x10, 0x10, 0x10, 0x0f,	/* U */
	0x07, 0x08, 0x10, 0x08, 0x07,	/* V */
	0x1f, 0x10, 0x0c, 0x10, 0x1f,	/* W */
	0x11, 0x0a, 0x04, 0x0a, 0x11,	/* X */
	0x01, 0x02, 0x1c, 0x02, 0x01,	/* Y */
	0x11, 0x19, 0x15, 0x13, 0x11,	/* Z */
	0x00, 0x1f, 0x11, 0x11, 0x00,	/* [ */
	0x01, 0x02, 0x04, 0x08, 0x10,	/* \ */
	0x00, 0x11, 0x11, 0x1f, 0x00,	/* ] */
	0x04, 0x02, 0x01, 0x02, 0x04,	/* ^ */
	0x10, 0x10, 0x10, 0x10, 0x10,	/* _ */

	0x00, 0x01, 0x02, 0x04, 0x00,	/* ` */
	0x08, 0x14, 0x14, 0x1c, 0x10,	/* a */
	0x1f, 0x14, 0x14, 0x14, 0x08,	/* b */
	0x0c, 0x12, 0x12, 0x12, 0x04,	/* c */
	0x08, 0x14, 0x14, 0x14, 0x1f,	/* d */
	0x1c, 0x2a, 0x2a, 0x2a, 0x0c,	/* e */
	0x00, 0x08, 0x3e, 0x09, 0x02,	/* f */
	0x48, 0x94, 0x94, 0x94, 0x68,	/* g */
	0x1f, 0x08, 0x04, 0x04, 0x18,	/* h */
	0x00, 0x10, 0x1d, 0x10, 0x00,	/* i */
	0x20, 0x40, 0x3d, 0x00, 0x00,	/* j */
	0x1f, 0x04, 0x0a, 0x10, 0x00,	/* k */
	0x00, 0x01, 0x3e, 0x20, 0x00,	/* l */
	0x1c, 0x04, 0x18, 0x04, 0x1c,	/* m */
	0x1c, 0x08, 0x04, 0x04, 0x18,	/* n */
	0x08, 0x14, 0x14, 0x14, 0x08,	/* o */
	0xfc, 0x14, 0x14, 0x14, 0x08,	/* p */
	0x08, 0x14, 0x14, 0xfc, 0x40,	/* q */
	0x1c, 0x08, 0x04, 0x04, 0x08,	/* r */
	0x10, 0x24, 0x2a, 0x2a, 0x10,	/* s */
	0x00, 0x04, 0x1f, 0x24, 0x00,	/* t */
	0x0c, 0x10, 0x10, 0x10, 0x0c,	/* u */
	0x04, 0x08, 0x10, 0x08, 0x04,	/* v */
	0x1c, 0x10, 0x0c, 0x10, 0x1c,	/* w */
	0x14, 0x08, 0x08, 0x08, 0x14,	/* x */
	0x4c, 0x90, 0x90, 0x90, 0x7c,	/* y */
	0x24, 0x34, 0x2c, 0x24, 0x00,	/* z */
	0x00, 0x04, 0x1b, 0x11, 0x00,	/* { */
	0x00, 0x00, 0x7f, 0x00, 0x00,	/* | */
	0x00, 0x11, 0x1b, 0x04, 0x00,	/* } */
	0x04, 0x02, 0x04, 0x08, 0x04,	/* ~ */
	0x7F, 0x7F, 0x7F, 0x7F, 0x7F	/* DEL */
};

/*
 * Internal - return the address of the character font's
 *            first byte (of 6).
 */
static const uchr_t *
lcd_fontchr(char c) {
	uchr_t ch = (uchr_t)c;
	unsigned offset;

	if ( ch <= ' ' || c > 0x7F )
		return &ascii_font[0];	/* Many map to blank */

	offset = (ch - 0x20) * 5;	/* Compute font cell */
	return &ascii_font[offset];	/* Return address of cell */
}

/*
 * Internal - restore the cursor:
 */
static void
lcd_restore(void) {
	int sety = 0x40 | (lcd_y & 0x07);
	int setx = (lcd_x * 6) | 0x80;

	lcd(LCD_Command);
	lcd_wr_byte(sety);
	lcd_wr_byte(setx);
	lcd(LCD_Unselect);
}

/*
 * Internal - scroll the text on the screen up one
 *            line.
 */
static void
lcd_scroll(void) {
	int y, x;

	lcd_home();	/* Home cursor */

	/* Scroll up text */
	for ( y=0; y<lcd_lines-1; ++y )
		for ( x=0; x<lcd_cols; ++x ) {
			lcd_buf[y][x] = lcd_buf[y+1][x];
			lcd_putraw(lcd_buf[y][x]);
		}

	/* Blank the last line */
	for ( x=0; x<lcd_cols; ++x ) {
		lcd_buf[lcd_lines-1][x] = ' ';
		lcd_putraw(' ');
	}

	lcd_restore();	/* Restore cursor */
}

/*
 * Internal - put one text character to LCD device
 */
static void
lcd_putraw(char c) {
	const uchr_t *fc = lcd_fontchr(c);	/* Locate font cell */
	int x;

	lcd(LCD_Data);				/* Start LCD data mode */
	for ( x=0; x<5; ++x )
		lcd_wr_byte(*fc++);		/* Send font bytes 0-4 */
	lcd_wr_byte(0x00);			/* And one blank cell */
	lcd(LCD_Unselect);
}

/*
 * Internal Enable/Disable for transmission
 */
void
lcd(Enable en) {
	int data, cs;

	switch ( en ) {
	case LCD_Unselect :
		data = cs = 0;			/* Unselect chip */
		break;
	case LCD_Command :
		data = 0;			/* Command is active low */
		cs = 1;				/* Chip select */
		break;
	case LCD_Data :
		data = 1;			/* Data is active high */
		cs = 1;				/* Chip select */
	}		

	if ( cs ) {				/* Chip select? */
		gpio_write(lcd_d_c,data);	/* Data or /command mode */
		gpio_write(lcd_ce,0);		/* Chip enable (active low) */
		gpio_write(lcd_sclk,0);		/* Set clock to low level */
		gpio_write(lcd_sdin,0);		/* Initially data to low for now */
	} else	{				/* Chip is being unselected */
		gpio_write(lcd_ce,1);		/* Disable chip enable */
		gpio_write(lcd_sclk,1);		/* Return sclk to high when unselected */
		gpio_write(lcd_d_c,1);		/* Ditto for data/command */
		gpio_write(lcd_sdin,1);		/* Ditto for data in */
	}
}

/*
 * Internal - Write one bit
 */
static void
lcd_wr_bit(int b) {
	gpio_write(lcd_sdin,b);
	gpio_write(lcd_sclk,1);
	gpio_write(lcd_sclk,0);
}

/*
 * Internal - Write one LCD byte (MSB first)
 */
static void
lcd_wr_byte(int byte) {
	int mask = 0x80;

	for ( ; mask > 0; mask >>= 1 )
		lcd_wr_bit((byte & mask) ? 1 : 0);
}

/*********************************************************************
 * Home cursor:
 *********************************************************************/
void
lcd_home(void) {
	lcd_move(0,0);
}

/*********************************************************************
 * Put one character onto the screen :
 *	CR moves cursor to column zero of current line.
 *	NL moves to next line, scrolling if ncessary, and implies CR.
 *********************************************************************/
void
lcd_putc(char c) {
	if ( c == '\r' ) {		/* CR - move cursor to col 0 */
		lcd_setx(lcd_x=0);
		return;
	} else if ( c == '\n' ) {	/* NL - move to next line */
		if ( ++lcd_y >= lcd_lines ) {
			lcd_scroll();
			lcd_y = lcd_lines-1;
		}
		lcd_move(lcd_y,lcd_x=0);
		return;
	}

	if ( lcd_x + 1 > lcd_cols ) {	/* Past column end? */
		if ( ++lcd_y >= lcd_lines ) {
			lcd_scroll();	/* Scroll if necessary */
			lcd_y = lcd_lines-1;
		}
		lcd_move(lcd_y,lcd_x=0);
	}

	lcd_putraw(c);
	lcd_buf[lcd_y][lcd_x++] = c;
}	

/*********************************************************************
 * Put string to LCD, interpreting CR and NL :
 *********************************************************************/
void
lcd_puts(const char *text) {
	while ( *text )
		lcd_putc(*text++);
}

/*********************************************************************
 * Configure LCD GPIO Pins and Initialize LCD
 *
 * Note:
 *	The LCD units vary in their ideal setting for Vop. Here the
 *	default value used is 0xBF, if the arg vop is <= 0. Good
 *	values vary from about 0xB0 all the way up to 0xE0.
 *
 * Hints:
 *	1. If you see a matrix of dark pixels, your Vop is too high.
 *	2. If you see faint or no pixels, increase Vop.
 *	3. For inverse video, readjust Vop to suit.
 *	4. Type '+' or '-' to try different Vop values. Press
 *	   + or - repeatedly until the contrast improves.
 *
 *********************************************************************/
void
lcd_init(int vop) {
	if ( vop > 0 )
		lcd_vop = vop;		/* Use this new value */

	/* No outputs yet.. */
	gpio_config(lcd_ce,Input);
	gpio_config(lcd_res,Input);
	gpio_config(lcd_d_c,Input);
	gpio_config(lcd_sdin,Input);
	gpio_config(lcd_sclk,Input);

	/* Configure all pins as high */
	gpio_write(lcd_ce,1);
	gpio_write(lcd_res,1);
	gpio_write(lcd_d_c,1);
	gpio_write(lcd_sdin,1);
	gpio_write(lcd_sclk,1);

	/* Now assert outputs */
	gpio_config(lcd_ce,Output);
	gpio_config(lcd_res,Output);
	gpio_config(lcd_d_c,Output);
	gpio_config(lcd_sdin,Output);
	gpio_config(lcd_sclk,Output);

	lcd(LCD_Command);	/* Command mode */

	gpio_write(lcd_res,0);	/* Apply /RESET */
	gpio_read(lcd_res);	/* Delay a little */
	gpio_read(lcd_res);	/* Delay a little */
	gpio_read(lcd_res);	/* Delay a little */

	gpio_write(lcd_res,1);	/* Deactivate /RESET */
	gpio_read(lcd_res);	/* Delay a little more */
	gpio_read(lcd_res);	/* Delay a little */
	gpio_read(lcd_res);	/* Delay a little */

	lcd_wr_byte(0x21);	/* Chip Active, Extended instructions enabled */
	lcd_wr_byte(lcd_vop);	/* Set Vop level */
	lcd_wr_byte(0x04);	/* Set TC */
	lcd_wr_byte(0x14);	/* Set Bias */
	lcd_wr_byte(0x20);	/* Chip Active, Extended instructions disabled */
	lcd_wr_byte(0x0C);	/* Set normal mode (adjust Vop if using inverse video) */
	lcd(LCD_Unselect);

	lcd_clear();		/* Clear screen */
}

/*********************************************************************
 * Set Y Address (Row: 0-5)
 *********************************************************************/
void
lcd_sety(int y) {
	int sety = 0x40 | (y & 0x07);

	lcd(LCD_Command);
	lcd_wr_byte(sety);
	lcd(LCD_Unselect);

	lcd_y = y;
}

/*********************************************************************
 * Set X Address (Col: 0-13)
 *********************************************************************/
void
lcd_setx(int x) {
	int setx = (x * 6) | 0x80;

	lcd(LCD_Command);
	lcd_wr_byte(setx);
	lcd(LCD_Unselect);

	lcd_x = x;
}

/*********************************************************************
 * Set y,x :
 *********************************************************************/
void
lcd_move(int y,int x) {
	int sety = 0x40 | (y & 0x07);
	int setx = (x * 6) | 0x80;

	lcd(LCD_Command);
	lcd_wr_byte(sety);
	lcd_wr_byte(setx);
	lcd(LCD_Unselect);

	lcd_y = y;
	lcd_x = x;
}

/*********************************************************************
 * Clear the display
 *********************************************************************/
void
lcd_clear(void) {
	lcd_home();
	lcd_clrtobot();
}

/*********************************************************************
 * Clear to end of screen from current position:
 *********************************************************************/
void
lcd_clrtobot(void) {
	int y, x, z, start_x;

	lcd_move(lcd_y,lcd_x);

	lcd(LCD_Data);

	start_x = lcd_x;
	for ( y=lcd_y; y<lcd_lines; ++y ) {
		for ( x=start_x; x<lcd_cols; ++x ) {
			for ( z=0; z<6; ++z )
				lcd_wr_byte(0x00);
		}
		lcd_buf[y][x] = ' ';
		start_x = 0;
	}

	lcd(LCD_Unselect);

	lcd_restore();
}

/*********************************************************************
 * Clear to end of line :
 *********************************************************************/
void
lcd_clrtoeol(void) {
	int x, z;

	lcd(LCD_Data);
	for ( x=lcd_x; x<lcd_cols; ++x ) {
		for ( z=0; z<6; ++z )
			lcd_wr_byte(0x00);
		lcd_buf[lcd_y][x] = ' ';
	}
	lcd(LCD_Unselect);
	lcd_restore();
}

/*********************************************************************
 * Format and display message 
 *********************************************************************/

void
lcd_printf(const char *format,...) {
	va_list ap;
	char buf[256];

	va_start(ap,format);
	vsnprintf(buf,sizeof buf,format,ap);
	va_end(ap);
	lcd_puts(buf);
}

/*********************************************************************
 * Return the current column position x
 *********************************************************************/
int
lcd_getx() {
	return lcd_x;
}

/*********************************************************************
 * Return the current row position y
 *********************************************************************/
int
lcd_gety() {
	return lcd_y;
}

/*********************************************************************
 * Return the current character at cursor
 *********************************************************************/
char
lcd_char() {
	if ( lcd_y >= lcd_lines || lcd_x >= lcd_cols )
		return 0x00;
	return lcd_buf[lcd_y][lcd_x];
}

/*********************************************************************
 * Interactive Demo Main Program
 *********************************************************************/

int
main(int argc,char **argv) {
	int tty = 0;				/* Use stdin */
	struct termios sv_ios, ios;
	int rc, quit;
	char ch;

 	rc = tcgetattr(tty,&sv_ios);		/* Save current settings */
	assert(!rc);
	ios = sv_ios;
	cfmakeraw(&ios);			/* Make into a raw config */
	ios.c_oflag = OPOST | ONLCR;		/* Keep output editing */
	rc = tcsetattr(tty,TCSAFLUSH,&ios);	/* Put terminal into raw mode */
	assert(!rc);

	/*
	 * Initialize and configure GPIO pins :
	 */
	gpio_init();
	lcd_init(0);
	lcd_clear();
	lcd_puts("Interactive\nDemo:\n: ");

	/*
	 * Process single character commands :
	 */
	for ( quit=0; !quit; ) {
		/*
		 * Prompt and read input char :
		 */
		write(1,": ",2);
		rc = read(tty,&ch,1);
		if ( rc != 1 ) 
			break;
		if ( islower(ch) )
			ch = toupper(ch);

		write(1,&ch,1);
		write(1,"\n",1);

		/*
		 * Process command char :
		 */
		switch ( ch ) {
		case 'C' :
			puts("C - clear & home.");
			lcd_clear();
			break;
		case 'X' :
			puts("X - putc('X')");
			lcd_putc('X');
			break;
		case 'M' :
			puts("M - Multi-line test message.");
			lcd_puts("Line 1\nLine 2.\n");
			break;
		case 'U' :
			puts("U - Cursor up.");
			rc = lcd_gety() - 1;
			if ( rc < 0 )
				rc = lcd_lines - 1;
			lcd_sety(rc);
			break;
		case 'D' :
			puts("D - Cursor down.");
			rc = lcd_gety() + 1;
			if ( rc >= lcd_lines )
				rc = 0;
			lcd_sety(rc);
			break;
		case 'L' :
			puts("L - Cursor left.");
			rc = lcd_getx() - 1;
			if ( rc < 0 )
				rc = lcd_cols - 1;
			lcd_setx(rc);
			break;
		case 'R' :
			puts("R - Cursor Right.");
			rc = lcd_getx() + 1;
			if ( rc >= lcd_cols )
				rc = 0;
			lcd_setx(rc);
			break;
		case 'E' :
			puts("E - Clear to eol.");
			lcd_clrtoeol();
			break;
		case 'S' :
			puts("S - Clear to end of screen.");
			lcd_clrtobot();
			break;
		case '!' :
			puts("! - Reset.");
			lcd_init(0);
			lcd_puts("Reset:\n");
			break;
		case '+' :
			lcd_init(++lcd_vop);
			printf("+ - Reset: Vop = %02X\n",lcd_vop);
			lcd_printf("Vop = 0x%02X\n",lcd_vop);
			break;
		case '-' :
			lcd_init(--lcd_vop);
			printf("+ - Reset: Vop = %02X\n",lcd_vop);
			lcd_printf("Vop = 0x%02X\n",lcd_vop);
			break;
		case 'Q' :			/* Quit */
			quit = 1;
			break;
		case '?' :
		case 'H' :
			puts(	"Menu:\n"
				"C - clear & home cursor\n"
				"X - putc('X')\n"
				"M - Multi-line test\n"
				"U - cursor Up\n"
				"D - cursor Down\n"
				"L - cursor Left\n"
				"R - cursor Right\n"
				"E - clear to end of line\n"
				"S - clear to end screen\n"
				"! - Reset LCD\n"
				"+ - Reset with increased Vop\n"
				"- - Reset with decreased Vop\n"
				"Q - Quit\n");
			break;
		case '\r' :
		case '\n' :
		case ' ' :
			lcd_putc(ch);
			break;
		default :			/* Unsupported */
			printf("Use '?' for menu. (%c)\n",ch);
			lcd_putc(ch);
		}
	}

	puts("\nExit.");

	tcsetattr(tty,TCSAFLUSH,&sv_ios);	/* Restore terminal mode */
	return 0;
}

/*********************************************************************
 * End pcd8844.c - by Warren W. Gay VE3WWG
 * This source code is placed into the public domain.
 *********************************************************************/
