#ifndef _USB_MIDI_H
#define _USB_MIDI_H

#include <light.h>

#include <stdint.h>

// TODO implement version fields properly
#define LUSB_COMMON_VERSION_STR           "0.1.0"

#define LUSB_COMMON_INFO_STR              "Light USB Common v" LUSB_VERSION_STR

Light_Module_Declare(light_usb_midi)

#ifdef _HAVE_TINYUSB
#include "bsp/board.h"
#include "tusb.h"
#endif

#endif