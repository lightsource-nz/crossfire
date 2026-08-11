#include <stdio.h>

#include <crossfire.h>
#include <module/mod_usbhost_midi.h>
#include <light_usbhost_midi.h>

#include <rend.h>
#include <light_canvas.h>
#include <light_ioport.h>
#include <light_display.h>
#include <light_display_po13.h>
#include <light_display_sh1107.h>
#include <module/mod_light_canvas.h>
#include <module/mod_light_display.h>
#include <module/mod_light_display_po13.h>

#include <TypeLightSans_ttf_16px_font.h>

#include "crossfire_internal.h"

// English
#define LANGUAGE_ID 0x0409
#define BUF_COUNT   4

#ifdef _HAVE_TINYUSB
#include <pico/time.h>
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
// owns the buffer handling and the region flushing that this file used to do by hand. left
// single-buffered and unpaced deliberately: the panel is 1KB, and this display is driven by
// events (a device mounting, a burst of MIDI) rather than by a clock
static struct canvas_context *display_canvas;

#ifdef _HAVE_TINYUSB
static volatile bool _usbhost_reset_pending = false;
#endif

static void crossfire_display_init(void);
static void crossfire_display_test_pattern(void);

static void crossfire_app_event(const struct light_module *mod, uint8_t event, void *arg);
static uint8_t crossfire_app_main(struct light_application *app);

Light_Application_Define(
        crossfire, crossfire_app_event, crossfire_app_main,
        &light_usbhost_midi,
        &rend,
        &light_canvas,
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

#ifdef _HAVE_TINYUSB
static void _usbhost_init(void)
{
        tusb_rhport_init_t host_init = {
                .role = TUSB_ROLE_HOST,
                .speed = TUSB_SPEED_AUTO
        };
        tusb_init(BOARD_TUH_RHPORT, &host_init);
}
#endif

void crossfire_init()
{

#ifdef _HAVE_TINYUSB
        // init tinyUSB board abstraction
        board_init();

        _usbhost_init();
        light_info("tinyUSB host stack initialized","");
#endif

#ifdef CF_HAVE_SPI_LINK
        // link-test rig role: both real hardware SPI peripherals are consumed by the
        // inter-board link (spi0 as Link OUT master, spi1 as Link IN slave -- see
        // crossfire_spi_link.c), so the display below is switched to a PIO-emulated SPI
        // master on the same pins instead of real spi1, which is unavailable
        cf_spi_link_init();
        cf_link_device_mount();
#endif
        crossfire_display_init();
}
void crossfire_usbhost_request_reset(void)
{
#ifdef _HAVE_TINYUSB
        _usbhost_reset_pending = true;
#endif
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

#ifdef CF_HAVE_SPI_LINK
        struct io_context *display_io = light_display_po13_setup_io_pio_spi_4p(PORT_PIO_SPI_0);
#else
        struct io_context *display_io = light_display_po13_setup_io_spi_4p(CF_DISPLAY_PORT_ID);
#endif

        display_main = light_display_po13_create_device("crossfire_display_main", display_io);
        light_display_set_render_context(display_main, display_render);
        // hardware column 0 maps to the bottom of the rotated (128x64) logical view
        // above and column (n_columns-1) to the top, so the driver's default
        // (chip-native ascending) sweep fills bottom-to-top -- reverse it so updates
        // read as filling top-to-bottom instead
        light_display_sh1107_set_sweep_direction(display_main, SH1107_SWEEP_REVERSE);

        // created after the device exists, since it presents onto it. &display_main serves
        // as the one-element device array -- light_canvas borrows the pointer and indexes
        // [0], and display_main is a file-static whose address is stable for the life of
        // the program
        display_canvas = light_canvas_create(display_render, &display_main, 1);

        crossfire_display_update_status();

        light_info("status display initialized","");
}
// RX/TX activity indicator geometry, in the logical (rotated) canvas -- shared by the
// drawing code below and by the region the indicator-only update pushes, so the two can't
// drift apart. two small squares on a third row below the text, fixed left=RX right=TX
#define CF_INDICATOR_SIZE       12
#define CF_INDICATOR_Y          (2 * TypeLightSans_ttf_16px_font.char_height + 4)
#define CF_INDICATOR_TX_X       20
#define CF_INDICATOR_RIGHT      (CF_INDICATOR_TX_X + CF_INDICATOR_SIZE)

static void _crossfire_display_redraw(bool indicators_only);

void crossfire_display_update_status(void)
{
        _crossfire_display_redraw(false);
}
// the indicators are by far the most frequently changing thing on this display -- they
// toggle on every burst of MIDI traffic -- and they occupy one narrow band of the panel.
// pushing only that band is what keeps their appearance/disappearance from visibly wiping
// across the glass: under the 90 degree rotation the band maps onto ~13 of the panel's 64
// hardware columns, so a sweep that used to cross the whole display now covers a strip
void crossfire_display_update_indicators(void)
{
        _crossfire_display_redraw(true);
}
static void _crossfire_display_redraw(bool indicators_only)
{
        if(!display_canvas)
                return;

        // opens the frame: because this canvas is single-buffered, that means waiting out
        // any update still reading the buffer -- the same cooperative drain this function
        // used to call for itself -- and then clearing. nothing blocks beyond that, which
        // matters because this runs from crossfire_task(), the same tick that services USB
        // and forwards MIDI
        if(!light_canvas_frame_begin(display_canvas))
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

        // unlabeled, since there's not much room to spare once the two text lines above
        // already use 2*char_height=38 of the 64px-tall logical canvas. only drawn while
        // cf_activity_indicators_service() considers that direction active -- rend has no
        // partial-region clear, so like the rest of this function, "off" just means not
        // drawing it into the freshly-cleared buffer
        if(cf_rx_indicator_active()) {
                rend_draw_rect(display_render,
                        (rend_point2d) {0, CF_INDICATOR_Y},
                        (rend_point2d) {CF_INDICATOR_SIZE, CF_INDICATOR_Y + CF_INDICATOR_SIZE},
                        true);
        }
        if(cf_tx_indicator_active()) {
                rend_draw_rect(display_render,
                        (rend_point2d) {CF_INDICATOR_TX_X, CF_INDICATOR_Y},
                        (rend_point2d) {CF_INDICATOR_RIGHT, CF_INDICATOR_Y + CF_INDICATOR_SIZE},
                        true);
        }
#endif

        // only the indicator band is marked dirty for an indicator-only redraw, which is
        // what keeps a burst of MIDI from visibly wiping the whole panel: under the 90
        // degree rotation that band covers ~13 of the 64 hardware columns. the rect is the
        // same CF_INDICATOR_* geometry the drawing above uses, so the two cannot drift
        if(indicators_only)
                light_canvas_invalidate(display_canvas,
                        (rend_point2d) {0, CF_INDICATOR_Y},
                        (rend_point2d) {CF_INDICATOR_RIGHT, CF_INDICATOR_Y + CF_INDICATOR_SIZE});
        else
                light_canvas_invalidate_all(display_canvas);

        // pushes asynchronously, same as before -- see the note in frame_begin above about
        // why nothing here may block
        light_canvas_frame_end(display_canvas);
}
void crossfire_task()
{
#ifdef _HAVE_TINYUSB
        // processes any pending USB host events (including newly-enumerated devices,
        // which surface as tuh_midi_mount_cb()/tuh_midi_umount_cb() calls, handled in
        // crossfire_forward.c); this does not block, since osal_queue_receive() ignores
        // its timeout under OPT_OS_NONE
        tuh_task();

        // handled here, after tuh_task() has fully returned, rather than inline from
        // whatever callback requested it -- see crossfire_usbhost_request_reset()'s
        // declaration for why. this does block the scheduler for a short, fixed window
        // (matches the settle delay TinyUSB's own dual/dynamic_switch example uses around
        // the same teardown/reinit sequence) -- acceptable here since it only ever runs
        // once per physical disconnect, not on any hot path
        if(_usbhost_reset_pending) {
                _usbhost_reset_pending = false;
                light_info("resetting USB host controller after device disconnect","");
                tusb_deinit(BOARD_TUH_RHPORT);
                sleep_ms(100);
                _usbhost_init();
        }
#endif
#ifdef CF_HAVE_MIDI_BACKEND
        cf_forward_service();
        // must run every tick, not just after cf_forward_service() sees new traffic --
        // it's what notices CF_ACTIVITY_INDICATOR_MS has elapsed and turns an indicator
        // back off again
        cf_activity_indicators_service();
#endif
}
