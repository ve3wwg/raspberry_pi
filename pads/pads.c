/*********************************************************************
 * pads.c : Examine GPIO Pads Control
 *********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define BCM2708_PERI_BASE   0x20000000
#define PADS_GPIO_BASE      (BCM2708_PERI_BASE + 0x100000)
#define PADS_GPIO_00_27     0x002C
#define PADS_GPIO_28_45     0x0030
#define PADS_GPIO_46_53     0x0034

#define GETPAD32(offset)    (*(unsigned *)((char *)(pads)+offset))

#define BLOCK_SIZE (4*1024)

volatile unsigned *pads;

void
initialize(void) {
    int mem_fd = open("/dev/mem",O_RDWR|O_SYNC);
    char *pads_map;

    if ( mem_fd <= 0 ) {
        perror("Opening /dev/mem");
        exit(1);
    }

    pads_map = (char *)mmap(
        NULL,             /* Any address */
        BLOCK_SIZE,       /* Map length */
        PROT_READ|PROT_WRITE,
        MAP_SHARED,       
        mem_fd,           /* File to map */
        PADS_GPIO_BASE    /* Offset to registers */
    );

    if ( (long)pads_map == -1L ) {
        perror("mmap failed.");
        exit(1);
    }

    close(mem_fd);
    pads = (volatile unsigned *)pads_map;
}

int
main(int argc,char **argv) {
    int x;
    union {
        struct {
            unsigned    drive : 3;
            unsigned    hyst : 1;
            unsigned    slew : 1;
            unsigned    reserved : 13;
            unsigned    passwrd : 8;
        }           s;
        unsigned    w;
    } word;

    initialize();

    for ( x=PADS_GPIO_00_27; x<=PADS_GPIO_46_53; x += 4 ) {
        word.w = GETPAD32(x);
        printf("%08X : %08X %x %x %x\n",
            x+0x7E10000,word.w,
            word.s.slew,word.s.hyst,word.s.drive);
    }

    return 0;
}

/*********************************************************************
 * End pads.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
