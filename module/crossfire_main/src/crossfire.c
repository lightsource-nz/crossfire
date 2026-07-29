#include <stdio.h>

#include <crossfire.h>
#include <module/mod_usbhost_midi.h>
#include <light_usbhost_midi.h>

#include <rend.h>
#include <light_display_ioport.h>
#include <light_display.h>
#include <light_display_po13.h>
#include <light_display_sh1107.h>
#include <module/mod_light_display.h>
#include <module/mod_light_display_po13.h>

#include <TypeLightSans_ttf_16px_font.h>

#include "crossfire_internal.h"

// English
#define LANGUAGE_ID 0x0409
#define BUF_COUNT   4

#ifdef _HAVE_TINYUSB
tusb_desc_device_t desc_device;
#endif

uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = { 0 }; // device address that owns buffer

// status OLED is a Pico-OLED-1.3 module (light_display_po13): its pins (GP6-12, SPI1)
// are fixed by the module itself, not configurable at the call site. this overlaps
// crossfire's onboard PIO-USB host ports 2 and 3 (GP6,9,10,11,12) -- confirmed
// acceptable: nothing should be plugged into those two connectors while the display
// is attached
#define CF_DISPLAY_PORT_ID       PORT_SPI_1

static struct rend_context *display_render;
static struct display_device *display_main;

static void crossfire_display_init(void);
static void crossfire_display_test_pattern(void);

static void crossfire_app_event(const struct light_module *mod, uint8_t event, void *arg);
static uint8_t crossfire_app_main(struct light_application *app);

Light_Application_Define(
        crossfire, crossfire_app_event, crossfire_app_main,
        &light_usbhost_midi,
        &rend,
        &light_display,
        &light_display_po13
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

        crossfire_display_init();
}
static void crossfire_display_init(void)
{
        display_render = rend_context_create(
                "crossfire_display", PO13_WIDTH, PO13_HEIGHT, 1);
        rend_context_set_font(display_render, &TypeLightSans_ttf_16px_font);
        // the panel is physically 64 wide x 128 tall, but text reads better run along
        // the long (128px) side -- rotate so the logical canvas callers draw against is
        // 128 wide x 64 tall instead; the SH1107 driver and PO13_WIDTH/HEIGHT are
        // unaffected, since rotation is purely a rend-side coordinate transform
        rend_context_set_rotation(display_render, REND_ROTATE_90);

        struct io_context *display_io = light_display_po13_setup_io_spi_4p(CF_DISPLAY_PORT_ID);

        display_main = light_display_po13_create_device("crossfire_display_main", display_io);
        light_display_set_render_context(display_main, display_render);
        // hardware column 0 maps to the bottom of the rotated (128x64) logical view
        // above and column (n_columns-1) to the top, so the driver's default
        // (chip-native ascending) sweep fills bottom-to-top -- reverse it so updates
        // read as filling top-to-bottom instead
        light_display_sh1107_set_sweep_direction(display_main, SH1107_SWEEP_REVERSE);

        crossfire_display_update_status();

        light_info("status display initialized","");
}
void crossfire_display_update_status(void)
{
        if(!display_main)
                return;

        // rend has no partial-region clear, so the whole buffer is cleared and both
        // lines redrawn together rather than trying to erase just the device count.
        // with the 90 degree rotation set in crossfire_display_init(), the logical
        // canvas here is 128 wide x 64 tall (TypeLightSans_ttf_16px_font is 12px/char
        // wide and 19px tall per line, so up to 10 characters fit per line and two
        // lines fit with room to spare), even though the panel is physically 64x128.
        // the font was rendered specifically for this panel's geometry (64x128 pixels
        // across its real 17.2x32.3mm glass, not an assumed square-pixel display) --
        // see font-crusher's 'po13' display object and the cmd_render_new__po13 test
        rend_draw_clear(display_render);
        rend_draw_text(display_render, (rend_point2d) {0, 0}, "Crossfire");

#ifdef CF_HAVE_MIDI_BACKEND
        uint8_t mounted_count = 0;
        for(uint8_t i = 0; i < CF_MAX_DEVICES; i++) {
                if(cf_midi_device[i].mounted)
                        mounted_count++;
        }
        uint8_t status_line[16];
        snprintf((char *)status_line, sizeof(status_line), "devices: %u", mounted_count);
        rend_draw_text(display_render, (rend_point2d) {0, TypeLightSans_ttf_16px_font.char_height}, status_line);
#endif

        light_display_command_update(display_main);
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
