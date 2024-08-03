#ifndef _USB_MIDI_H
#define _USB_MIDI_H

#include <light.h>

#include <stdint.h>

// TODO implement version fields properly
#define LUSB_MIDI_VERSION_STR           "0.1.0"

#define LUSB_MIDI_INFO_STR              "Light USB MIDI v" LUSB_MIDI_VERSION_STR

Light_Module_Declare(light_usb_midi)

#endif