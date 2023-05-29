#include <crossfire.h>
#include <module/usb_common.h>

#include "crossfire_internal.h"

// English
#define LANGUAGE_ID 0x0409
#define BUF_COUNT   4

#ifdef _HAVE_TINYUSB
tusb_desc_device_t desc_device;
#endif

uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = { 0 }; // device address that owns buffer

static void crossfire_app_event(const struct light_module *mod, uint8_t event);

Light_Application_Define(crossfire, crossfire_app_event, &light_usb_common, &light_framework);

void main()
{
        light_framework_init();

#ifdef _HAVE_TINYUSB
        // init tinyUSB board abstraction
        board_init();

        tuh_init(BOARD_TUH_RHPORT);
#endif
    
        __breakpoint();
}

static void crossfire_app_event(const struct light_module *mod, uint8_t event)
{
        switch (event) {
        case LF_EVENT_LOAD:
                light_debug("crossfire app module received LOAD event","");
                break;
        case LF_EVENT_UNLOAD:
                light_debug("crossfire app module received UNLOAD event","");
                break;
        }
}
