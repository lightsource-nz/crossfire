#include <crossfire.h>
#include <module/mod_usbhost_midi.h>
#include <light_usbhost_midi.h>

#include "crossfire_internal.h"

// English
#define LANGUAGE_ID 0x0409
#define BUF_COUNT   4

#ifdef _HAVE_TINYUSB
tusb_desc_device_t desc_device;
#endif

uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = { 0 }; // device address that owns buffer

static void crossfire_app_event(const struct light_module *mod, uint8_t event, void *arg);
static uint8_t crossfire_app_main(struct light_application *app);

Light_Application_Define(
        crossfire, crossfire_app_event, crossfire_app_main,
        &light_usbhost_midi
);

int main(int argc, char *argv[])
{
        light_framework_init();
        light_framework_run(argc, argv);

        return LIGHT_OK;
}

static void crossfire_app_event(const struct light_module *mod, uint8_t event, void *arg)
{
        switch (event) {
        case LF_EVENT_MODULE_LOAD:
                light_debug("crossfire app module received LOAD event","");
                crossfire_init();
                break;
        case LF_EVENT_MODULE_UNLOAD:
                light_debug("crossfire app module received UNLOAD event","");
                break;
        }
}

static uint8_t crossfire_app_main(struct light_application *app)
{
        crossfire_task();
        return LF_STATUS_RUN;
}

void crossfire_init()
{

#ifdef _HAVE_TINYUSB
        // init tinyUSB board abstraction
        board_init();

        tusb_rhport_init_t host_init = {
                .role = TUSB_ROLE_HOST,
                .speed = TUSB_SPEED_AUTO
        };
        tusb_init(BOARD_TUH_RHPORT, &host_init);
        light_info("tinyUSB host stack initialized","");
#endif
}
void crossfire_task()
{
#ifdef _HAVE_TINYUSB
        // processes any pending USB host events (including newly-enumerated devices,
        // which surface as tuh_midi_mount_cb()/tuh_midi_umount_cb() calls, handled in
        // crossfire_forward.c); this does not block, since osal_queue_receive() ignores
        // its timeout under OPT_OS_NONE
        tuh_task();
#endif
#ifdef CF_HAVE_MIDI_BACKEND
        cf_forward_service();
#endif
}
