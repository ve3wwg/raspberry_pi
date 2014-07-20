/*********************************************************************
 * valt.c : Read and report configuration for each GPIO
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

static struct {
	int		gpio;
	const char 	*pullup;	
	const char	*alt[6];
	const char 	*rev2_name;
} gpio_funcs[] = {
	{ 0,
	  "High",
	  { "SDA0", "SA5", "<reserved>", "-", "-", "-", },
	  "SDA0",
	},
	{ 1,
	  "High",
	  { "SCL0", "SA4", "<reserved>", "-", "-", "-", },
	  "SCL0",
	},
	{ 2,
	  "High",
	  { "SDA1", "SA3", "<reserved>", "-", "-", "-", },
	  "SDA1",
	},
	{ 3,
	  "High",
	  { "SCL1", "SA2", "<reserved>", "-", "-", "-", },
	  "SCL1",
	},
	{ 4,
	  "High",
	  { "GPCLK0", "SA1", "<reserved>", "-", "-", "ARM_TDI", },
	  "GPIO_GCLK",
	},
	{ 5,
	  "High",
	  { "GPCLK1", "SA0", "<reserved>", "-", "-", "ARM_TDO", },
	  "CAM_CLK",
	},
	{ 6,
	  "High",
	  { "GPCLK2", "SOE_N/SE", "<reserved>", "-", "-", "ARM_RTCK", },
	  "LAN_RUN",
	},
	{ 7,
	  "High",
	  { "SPI0_CE1_N", "SWE_N/SRW_N", "<reserved>", "-", "-", "-", },
	  "SPI_CE1_N",
	},
	{ 8,
	  "High",
	  { "SPI0_CE0_N", "SD0", "<reserved>", "-", "-", "-", },
	  "SPI_CE0_N",
	},
	{ 9,
	  "Low",
	  { "SPI0_MISO", "SD1", "<reserved>", "-", "-", "-", },
	  "SPI_MISO",
	},
	{ 10,
	  "Low",
	  { "SPI0_MOSI", "SD2", "<reserved>", "-", "-", "-", },
	  "SPI_MOSI",
	},
	{ 11,
	  "Low",
	  { "SPI0_SCLK", "SD3", "<reserved>", "-", "-", "-", },
	  "SPI_SCLK",
	},
	{ 12,
	  "Low",
	  { "PWM0", "SD4", "<reserved>", "-", "-", "ARM_TMS", },
	  "nc",
	},
	{ 13,
	  "Low",
	  { "PWM1", "SD5", "<reserved>", "-", "-", "ARM_TCK", },
	  "nc",
	},
	{ 14,
	  "Low",
	  { "TXD0", "SD6", "<reserved>", "-", "-", "TXD1", },
	  "TXD0",
	},
	{ 15,
	  "Low",
	  { "RXD0", "SD7", "<reserved>", "-", "-", "RXD1", },
	  "RXD0",
	},
	{ 16,
	  "Low",
	  { "<reserved>", "SD8", "<reserved>", "CTS0", "SPI1_CE2_N", "CTS1", },
	  "STATUS_LED_N",
	},
	{ 17,
	  "Low",
	  { "<reserved>", "SD9", "<reserved>", "RTS0", "SPI1_CE1_N", "RTS1", },
	  "GPIO_GEN0",
	},
	{ 18,
	  "Low",
	  { "PCM_CLK", "SD10", "<reserved>", "BSCSL-SDA/MOSI", "SPI1_CE0_N", "PWM0", },
	  "GPIO_GEN1",
	},
	{ 19,
	  "Low",
	  { "PCM_FS", "SD11", "<reserved>", "BSCSL-SCL/SCLK", "SPI1_MISO", "PWM1", },
	  "nc",
	},
	{ 20,
	  "Low",
	  { "PCM_DIN", "SD12", "<reserved>", "BSCSL/MISO", "SPI1_MOSI", "GPCLK0", },
	  "nc",
	},
	{ 21,
	  "Low",
	  { "PCM_DOUT", "SD13", "<reserved>", "BSCSL/CE_N", "SPI1_SCLK", "GPCLK1", },
	  "CAM_GPIO",
	},
	{ 22,
	  "Low",
	  { "<reserved>", "SD14", "<reserved>", "SD1_CLK", "ARM_TRST", "-", },
	  "GPIO_GEN3",
	},
	{ 23,
	  "Low",
	  { "<reserved>", "SD15", "<reserved>", "SD1_CMD", "ARM_RTCK", "-", },
	  "GPIO_GEN4",
	},
	{ 24,
	  "Low",
	  { "<reserved>", "SD16", "<reserved>", "SD1_DAT0", "ARM_TDO", "-", },
	  "GPIO_GEN5",
	},
	{ 25,
	  "Low",
	  { "<reserved>", "SD17", "<reserved>", "SD1_DAT1", "ARM_TCK", "-", },
	  "GPIO_GEN6",
	},
	{ 26,
	  "Low",
	  { "<reserved>", "<reserved>", "<reserved>", "SD1_DAT2", "ARM_TDI", "-", },
	  "nc",
	},
	{ 27,
	  "Low",
	  { "<reserved>", "<reserved>", "<reserved>", "SD1_DAT3", "ARM_TMS", "-", },
	  "GPIO_GEN2",
	},
	{ 28,
	  "-",
	  { "SDA0", "SA5", "PCM_CLK", "<reserved>", "-", "-", },
	  "GPIO_GEN7",
	},
	{ 29,
	  "-",
	  { "SCL0", "SA4", "PCM_FS", "<reserved>", "-", "-", },
	  "GPIO_GEN8",
	},
	{ 30,
	  "Low",
	  { "<reserved>", "SA3", "PCM_DIN", "CTS0", "-", "CTS1", },
	  "GPIO_GEN9",
	},
	{ 31,
	  "Low",
	  { "<reserved>", "SA2", "PCM_DOUT", "RTS0", "-", "RTS1", },
	  "GPIO_GEN10",
	},
	{ 32,
	  "Low",
	  { "GPCLK0", "SA1", "<reserved>", "TXD0", "-", "TXD1", },
	  "nc",
	},
	{ 33,
	  "Low",
	  { "<reserved>", "SA0", "<reserved>", "RXD0", "-", "RXD1", },
	  "nc",
	},
	{ 34,
	  "High",
	  { "GPCLK0", "SOE_N/SE", "<reserved>", "<reserved>", "-", "-", },
	  "nc",
	},
	{ 35,
	  "High",
	  { "SPI0_CE1_N", "SWE_N/SRW_N", "-", "<reserved>", "-", "-", },
	  "nc",
	},
	{ 36,
	  "High",
	  { "SPI0_CE0_N", "SD0", "TXD0", "<reserved>", "-", "-", },
	  "nc",
	},
	{ 37,
	  "Low",
	  { "SPI0_MISO", "SD1", "RXD0", "<reserved>", "-", "-", },
	  "nc",
	},
	{ 38,
	  "Low",
	  { "SPI0_MOSI", "SD2", "RTS0", "<reserved>", "-", "-", },
	  "nc",
	},
	{ 39,
	  "Low",
	  { "SPI0_SCLK", "SD3", "CTS0", "<reserved>", "-", "-", },
	  "nc",
	},
	{ 40,
	  "Low",
	  { "PWM0", "SD4", "-", "<reserved>", "SPI2_MISO", "TXD1", },
	  "PWM0_OUT",
	},
	{ 41,
	  "Low",
	  { "PWM1", "SD5", "<reserved>", "<reserved>", "SPI2_MOSI", "RXD1", },
	  "nc",
	},
	{ 42,
	  "Low",
	  { "GPCLK1", "SD6", "<reserved>", "<reserved>", "SPI2_SCLK", "RTS1", },
	  "nc",
	},
	{ 43,
	  "Low",
	  { "GPCLK2", "SD7", "<reserved>", "<reserved>", "SPI2_CE0_N", "CTS1", },
	  "nc",
	},
	{ 44,
	  "-",
	  { "GPCLK1", "SDA0", "SDA1", "<reserved>", "SPI2_CE1_N", "-", },
	  "nc",
	},
	{ 45,
	  "-",
	  { "PWM1", "SCL0", "SCL1", "<reserved>", "SPI2_CE2_N", "-", },
	  "PWM1_OUT",
	},
};

static inline unsigned
gpio_get_alt(unsigned gpio) {
	unsigned a, ur = *(ugpio+(((gpio)/10)));

	a = (ur >> (gpio % 10) * 3) & 7;
	if ( a >= 4 )
		return a - 4;
	if ( a >= 2 )
		return a == 3 ? 4 : 5;
	return a + 6;
}

int
main(int argc,char **argv) {
	unsigned a, b;
	int x;

	gpio_init();    			/* Initialize GPIO access */

	for ( x=0; x<46; ++x ) {
		a = gpio_get_alt(x);
		b = gpio_read(x);
		printf("GPIO %02d  %-4s ",x,gpio_funcs[x].pullup);
		if ( a < 6 ) {
			printf("%-10s (ALT %u)  ",gpio_funcs[x].alt[a],a);
		} else	printf("%-10s          ",a == 6 ? "Input" : "Output");
		printf("state = %u\n",b);
	}

	return 0;
}

/*********************************************************************
 * End valt.c - by Warren Gay
 * Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
