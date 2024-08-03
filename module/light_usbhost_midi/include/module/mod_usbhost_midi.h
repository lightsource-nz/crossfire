#ifndef _MOD_USBHOST_MIDI_H
#define _MOD_USBHOST_MIDI_H

#include <light.h>

#include <stdint.h>

// TODO implement version fields properly
#define LUSB_HOST_MIDI_VERSION_STR           "0.1.0"

#define LUSB_HOST_MIDI_INFO_STR              "Light USB Host v" LUSB_HOST_MIDI_VERSION_STR

Light_Module_Declare(light_usbhost_midi)

#endif