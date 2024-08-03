#ifndef _LIGHT_USBHOST_MIDI_H
#define _LIGHT_USBHOST_MIDI_H

#include <light_usb_midi.h>
#include <light_usbhost.h>

#if(LIGHT_SYSTEM == SYSTEM_PICO_SDK && LIGHT_PLATFORM == PLATFORM_TARGET)
#define _HAVE_TINYUSB
#include "bsp/board.h"
#include "tusb.h"
#endif

extern void light_usbhost_midi_init();

#endif