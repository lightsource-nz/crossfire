#ifdef CF_HAVE_MIDI_BACKEND

#include <light.h>
#include <light_platform.h>

#include "crossfire_internal.h"
#include "crossfire_midi_backend.h"

//   the activity LED. On the Pico this is the on-board LED; a flat RP2 GPIO number, as
// light_platform's GPIO API takes (a port to STM32 supplies its own, via
// LIGHT_IOPORT_PIN_STM32()).
#define CF_MIDI_LED_PIN        25

//   CONFIGURED ON FIRST USE, which it never was before. Nothing in this project ever set the
// pin's direction: it worked only because TinyUSB's RP2040 BSP configures GP25 as its own board
// LED, so board_init() happened to leave it an output. That is not a dependency worth keeping --
// it makes the LED work on exactly the boards whose BSP shares our pin choice, and silently
// nothing anywhere else.
static void _midi_led_write(bool on)
{
        static bool configured;
        if(!configured) {
                light_platform_gpio_configure_output(CF_MIDI_LED_PIN, on);
                configured = true;
                return;
        }
        light_platform_gpio_write(CF_MIDI_LED_PIN, on);
}

struct cf_midi_device cf_midi_device[CF_MAX_DEVICES];
struct cf_forward_list cf_forward_table[CF_MAX_DEVICES][CF_MAX_CABLES_PER_DEVICE];

// timestamps of the most recent RX/TX activity, and whether the indicator each currently
// drives is on-screen right now -- the "shown" flags are the single source of truth
// cf_rx_indicator_active()/cf_tx_indicator_active() read back, kept separate from the raw
// timestamps so a redraw is only triggered on an actual on/off transition, not every tick
static uint32_t cf_last_rx_ms;
static uint32_t cf_last_tx_ms;
static bool cf_rx_indicator_shown;
static bool cf_tx_indicator_shown;

bool cf_rx_indicator_active(void)
{
        return cf_rx_indicator_shown;
}
bool cf_tx_indicator_active(void)
{
        return cf_tx_indicator_shown;
}
void cf_activity_indicators_service(void)
{
        uint32_t now = light_platform_get_time_since_init();
        bool rx_active = (now - cf_last_rx_ms) < CF_ACTIVITY_INDICATOR_MS;
        bool tx_active = (now - cf_last_tx_ms) < CF_ACTIVITY_INDICATOR_MS;

        if(rx_active == cf_rx_indicator_shown && tx_active == cf_tx_indicator_shown)
                return;

        cf_rx_indicator_shown = rx_active;
        cf_tx_indicator_shown = tx_active;
        // only an indicator changed here by construction (the early-out above returns
        // unless one of the two flags flipped), so only that band needs pushing
        crossfire_display_update_indicators();
}

void cf_forward_table_rebuild(void)
{
        for(uint8_t src_idx = 0; src_idx < CF_MAX_DEVICES; src_idx++) {
                for(uint8_t cable = 0; cable < CF_MAX_CABLES_PER_DEVICE; cable++) {
                        cf_forward_table[src_idx][cable].count = 0;
                }
                if(!cf_midi_device[src_idx].mounted)
                        continue;

                uint8_t src_cables = cf_midi_device[src_idx].rx_cable_count;
                if(src_cables > CF_MAX_CABLES_PER_DEVICE)
                        src_cables = CF_MAX_CABLES_PER_DEVICE;

                for(uint8_t cable = 0; cable < src_cables; cable++) {
                        struct cf_forward_list *list = &cf_forward_table[src_idx][cable];
                        for(uint8_t dst_idx = 0; dst_idx < CF_MAX_DEVICES; dst_idx++) {
                                if(dst_idx == src_idx || !cf_midi_device[dst_idx].mounted)
                                        continue;
                                if(cable >= cf_midi_device[dst_idx].tx_cable_count)
                                        continue;
                                list->entry[list->count].dst_idx = dst_idx;
                                list->entry[list->count].dst_cable = cable;
                                list->count++;
                        }
                }
        }
}

// dispatches a single 4-byte USB-MIDI Event Packet read/write to whichever transport
// backs this device slot -- real USB devices go through tinyusb's packet API, the
// SPI-linked peer (if compiled in) goes through crossfire_spi_link.c. keeping this
// dispatch in one place is what lets cf_forward_service() below treat every device slot
// uniformly, regardless of what's actually behind it
static bool cf_device_packet_read(uint8_t idx, uint8_t packet[4])
{
        if(cf_midi_device[idx].kind == CF_DEVICE_KIND_USB)
                return tuh_midi_packet_read(idx, packet);
#ifdef CF_HAVE_SPI_LINK
        if(cf_midi_device[idx].kind == CF_DEVICE_KIND_SPI_LINK)
                return cf_spi_link_packet_read(packet);
#endif
        return false;
}
static void cf_device_packet_write(uint8_t idx, const uint8_t packet[4])
{
        if(cf_midi_device[idx].kind == CF_DEVICE_KIND_USB) {
                tuh_midi_packet_write(idx, packet);
                return;
        }
#ifdef CF_HAVE_SPI_LINK
        if(cf_midi_device[idx].kind == CF_DEVICE_KIND_SPI_LINK) {
                cf_spi_link_packet_write(packet);
                return;
        }
#endif
}

void cf_forward_service(void)
{
        uint8_t packet[4];
        bool wrote_any[CF_MAX_DEVICES] = { 0 };

        for(uint8_t src_idx = 0; src_idx < CF_MAX_DEVICES; src_idx++) {
                if(!cf_midi_device[src_idx].mounted)
                        continue;

                // drain every packet available from this source before moving to the next
                // slot -- same "loop until empty" contract the old stream-based version had
                while(cf_device_packet_read(src_idx, packet)) {
                        cf_last_rx_ms = light_platform_get_time_since_init();
                        uint8_t cable_num = packet[0] >> 4;
                        if(cable_num >= CF_MAX_CABLES_PER_DEVICE)
                                continue;
                        struct cf_forward_list *list = &cf_forward_table[src_idx][cable_num];
                        for(uint8_t i = 0; i < list->count; i++) {
                                struct cf_forward_entry *fwd = &list->entry[i];
                                // re-embed the destination's own cable number in byte 0,
                                // replacing the source's -- the CIN (low nibble) and MIDI
                                // data bytes pass through unchanged
                                uint8_t out_packet[4] = {
                                        (uint8_t)((fwd->dst_cable << 4) | (packet[0] & 0x0F)),
                                        packet[1], packet[2], packet[3]
                                };
                                cf_device_packet_write(fwd->dst_idx, out_packet);
                                wrote_any[fwd->dst_idx] = true;
                        }
                }
        }
        for(uint8_t idx = 0; idx < CF_MAX_DEVICES; idx++) {
                if(!wrote_any[idx])
                        continue;
                cf_last_tx_ms = light_platform_get_time_since_init();
                // the SPI link has no separate flush step -- each packet write is already
                // a complete, immediate CS-framed burst
                if(cf_midi_device[idx].kind == CF_DEVICE_KIND_USB)
                        tuh_midi_write_flush(idx);
        }
}

#ifdef CF_HAVE_SPI_LINK
void cf_link_device_mount(void)
{
        cf_midi_device[CF_LINK_DEVICE_IDX].mounted = true;
        cf_midi_device[CF_LINK_DEVICE_IDX].kind = CF_DEVICE_KIND_SPI_LINK;
        cf_midi_device[CF_LINK_DEVICE_IDX].daddr = 0; // no real USB address -- not tinyusb-backed
        cf_midi_device[CF_LINK_DEVICE_IDX].rx_cable_count = 1;
        cf_midi_device[CF_LINK_DEVICE_IDX].tx_cable_count = 1;
        light_info("SPI-linked peer registered as device idx=%d", CF_LINK_DEVICE_IDX);
        cf_forward_table_rebuild();
}
#endif

void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t *mount_cb_data)
{
        if(idx >= CF_MAX_DEVICES)
                return;
        cf_midi_device[idx].mounted = true;
        cf_midi_device[idx].kind = CF_DEVICE_KIND_USB;
        cf_midi_device[idx].daddr = mount_cb_data->daddr;
        cf_midi_device[idx].rx_cable_count = mount_cb_data->rx_cable_count;
        cf_midi_device[idx].tx_cable_count = mount_cb_data->tx_cable_count;
        light_info("USB-MIDI device mounted: idx=%d daddr=%d rx_cables=%d tx_cables=%d",
                        idx, mount_cb_data->daddr, mount_cb_data->rx_cable_count, mount_cb_data->tx_cable_count);
        cf_forward_table_rebuild();
        _midi_led_write(true);
        crossfire_display_update_status();
}
void tuh_midi_umount_cb(uint8_t idx)
{
        if(idx >= CF_MAX_DEVICES)
                return;
        light_info("USB-MIDI device unmounted: idx=%d daddr=%d", idx, cf_midi_device[idx].daddr);
        cf_midi_device[idx].mounted = false;
        cf_forward_table_rebuild();
        _midi_led_write(false);
        crossfire_display_update_status();
        // see crossfire_usbhost_request_reset()'s declaration for why this is needed --
        // works around a stale-hardware-state panic on RP2040's native USB host controller
        // that otherwise breaks reconnecting the same device
        crossfire_usbhost_request_reset();
}

#endif // CF_HAVE_MIDI_BACKEND
