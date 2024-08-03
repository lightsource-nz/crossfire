#ifndef _LIGHT_USBHOST_H
#define _LIGHT_USBHOST_H

#include <light_usb.h>

struct light_usbhost_port {

};

extern void light_usbhost_init();

extern uint8_t light_usbhost_port_init(uint8_t port_id);

#endif