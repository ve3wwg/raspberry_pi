/*********************************************************************
 * tusb.c - Scan list of USB devices and test claim/release.
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <usb.h>
#include <assert.h>

/*********************************************************************
 * See http://libusb.sourceforge.net/doc/index.html for API
 *********************************************************************/

int
main(int argc,char **argv) {
    struct usb_bus *busses, *bus;
    struct usb_device *dev;
    struct usb_device_descriptor *desc;
    usb_dev_handle *hdev;
    int cx, ix, ax, rc;
	
    usb_init();
    usb_find_busses();
    usb_find_devices();

    busses = usb_get_busses();

    for ( bus=busses; bus; bus = bus->next ) {
        for ( dev=bus->devices; dev; dev = dev->next ) {
            desc = &dev->descriptor;

            printf("Device: %s %04x:%04x ",
                dev->filename,
                desc->idVendor,
                desc->idProduct);
            printf("  class %u.%d protocol %u",
                desc->bDeviceClass,
                desc->bDeviceSubClass,
                desc->bDeviceProtocol);
            printf(" device %u, manuf %u, serial %u\n",
                desc->bcdDevice,
                desc->iManufacturer,
                desc->iSerialNumber);

            hdev = usb_open(dev);
            assert(hdev);
            
            rc = usb_claim_interface(hdev,0);
            if ( !rc ) {
                puts("  CLAIMED..");
                rc = usb_release_interface(hdev,0);
                puts("  RELEASED..");
                assert(!rc);
            }
            usb_close(hdev);
            
            /* Configurations */
            for ( cx=0; cx < dev->descriptor.bNumConfigurations; ++cx) {
                /* Interfaces */
                for ( ix=0; ix < dev->config[cx].bNumInterfaces; ++ix ) {
                    /* Alternates */
                    for ( ax=0; ax < dev->config[cx].interface[ix].num_altsetting; ++ax ) {
                        printf("  %d.%d.%d class %u\n",
                        cx,ix,ax,
                        dev->config[cx].interface[ix].altsetting[ax].bInterfaceClass);
                    }
                }
            }
        }
    }

    return 0;
}

/*********************************************************************
 * End tusb.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
   
