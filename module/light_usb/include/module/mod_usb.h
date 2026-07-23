#ifndef _MOD_USB_H
#define _MOD_USB_H

#include <light.h>

// TODO implement version fields properly
#define LUSB_VERSION_STR           "0.1.0"

#define LUSB_INFO_STR              "Light USB Common v" LUSB_VERSION_STR

Light_Module_Declare(light_usb);

#endif