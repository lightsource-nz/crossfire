#include <light.h>
#include <module/mod_usb.h>
#include <module/mod_usbhost.h>
#include <light_usbhost.h>

static void _event_load(const struct light_module *module)
{
        light_usbhost_init();
}
static void _event_unload(const struct light_module *module)
{
        
}
static void _module_event(const struct light_module *module, uint8_t event)
{
        switch(event) {
        case LF_EVENT_LOAD:
                _event_load(module);
                break;
        case LF_EVENT_UNLOAD:
                _event_unload(module);
                break;
        }
}
Light_Module_Define(light_usbhost, _module_event,
                                                &light_usb,
                                                &light_framework);
