#include <light_usbhost_midi.h>

#if(LIGHT_SYSTEM == SYSTEM_PICO_SDK && LIGHT_PLATFORM == PLATFORM_TARGET)
#define _HAVE_TINYUSB
#include "bsp/board.h"
#include "tusb.h"
#endif

void light_usbhost_midi_init()
{

}