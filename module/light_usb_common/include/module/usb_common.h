#ifndef _USB_COMMON_H
#define _USB_COMMON_H

#include <light.h>

#include <stdint.h>

// TODO implement version fields properly
#define LUSB_COMMON_VERSION_STR           "0.1.0"

#define LUSB_COMMON_INFO_STR              "Light USB Common v" LUSB_VERSION_STR

Light_Module_Declare(light_usb_common)

#if(LIGHT_SYSTEM == SYSTEM_PICO_SDK && LIGHT_PLATFORM == PLATFORM_TARGET)
#define _HAVE_TINYUSB
#include "bsp/board.h"
#include "tusb.h"
#endif

#endif