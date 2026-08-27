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

uint8_t cf_usb_mounted_count(void)
{
        uint8_t count = 0;
        // CF_MAX_DEVICES_USB, not CF_MAX_DEVICES: the SPI link's reserved slot lives past the
        // end of this range and is not on the USB bus at all
        for(uint8_t idx = 0; idx < CF_MAX_DEVICES_USB; idx++) {
                if(cf_midi_device[idx].mounted && cf_midi_device[idx].kind == CF_DEVICE_KIND_USB)
                        count++;
        }
        return count;
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
                        //   DROP RESERVED CODE INDEX NUMBERS, which in practice means dropping
                        // padding. A USB-MIDI bulk transfer carries 16 four-byte event slots and
                        // a device that has only one event to send zero-fills the other 15; CIN
                        // 0x0 and 0x1 are reserved by USB-MIDI 1.0 and never carry data, so a
                        // zero-filled slot is padding rather than a message. Whether those slots
                        // are handed back at all depends on the host stack -- the two TinyUSB
                        // versions in use here differ, which is why this only showed up in one
                        // direction: pico-sdk's bundled copy returns them, the standalone
                        // checkout the H7 builds against does not.
                        //   forwarding them was not harmless. Every padding slot was a 4-byte
                        // burst on the inter-board SPI link, so up to 15 of every 16 bytes the
                        // link carried were nothing at all, and the receiving end reassembled
                        // them into packets and passed them on as MIDI. It only became visible
                        // once the link started checking that what it assembled was plausible.
                        uint8_t cin = packet[0] & 0x0F;
                        if(cin < 0x2)
                                continue;
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

//   CF_MAX_DEVICES_USB, not CF_MAX_DEVICES. The two are the same number until the SPI link is
// compiled in, and then CF_MAX_DEVICES is one larger -- and that extra slot is the link's, not
// something a USB mount callback may ever land in. tinyusb cannot hand out an index that high
// (CFG_TUH_MIDI is CF_MAX_DEVICES_USB), so this is a guard against the two constants drifting,
// not against tinyusb
void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t *mount_cb_data)
{
        if(idx >= CF_MAX_DEVICES_USB)
                return;
        cf_midi_device[idx].mounted = true;
        cf_midi_device[idx].kind = CF_DEVICE_KIND_USB;
        cf_midi_device[idx].daddr = mount_cb_data->daddr;
        cf_midi_device[idx].rx_cable_count = mount_cb_data->rx_cable_count;
        cf_midi_device[idx].tx_cable_count = mount_cb_data->tx_cable_count;
        light_info("USB-MIDI device mounted: idx=%d daddr=%d rx_cables=%d tx_cables=%d",
                        idx, mount_cb_data->daddr, mount_cb_data->rx_cable_count, mount_cb_data->tx_cable_count);
#ifdef CF_HAVE_USB_HUB
        // after the slot is filled in, since the port map is keyed by the same index and
        // cf_hub_port_occupied() reads .mounted back
        cf_hub_device_attached(idx, mount_cb_data->daddr);
#endif
        cf_forward_table_rebuild();
        _midi_led_write(true);
        crossfire_display_update_status();
}
void tuh_midi_umount_cb(uint8_t idx)
{
        if(idx >= CF_MAX_DEVICES_USB)
                return;
        light_info("USB-MIDI device unmounted: idx=%d daddr=%d", idx, cf_midi_device[idx].daddr);
        cf_midi_device[idx].mounted = false;
#ifdef CF_HAVE_USB_HUB
        // after .mounted is cleared: this is what decides whether the hub itself is gone, and
        // it asks cf_usb_mounted_count()
        cf_hub_device_detached(idx);
#endif
        cf_forward_table_rebuild();
        // the LED tracks "anything mounted", not "the last event was a mount" -- with a hub in
        // front of the port, pulling one instrument out of four used to go dark
        _midi_led_write(cf_usb_mounted_count() != 0);
        crossfire_display_update_status();
        //   ONLY WHEN THE ROOT PORT IS NOW EMPTY. See crossfire_usbhost_request_reset()'s
        // declaration: it works around a stale-hardware-state panic on RP2040's native USB host
        // controller that otherwise breaks reconnecting a device, but it does so by resetting
        // the entire controller -- which also drops every other device on the bus. Behind a hub
        // that would mean unplugging one instrument silently killed the other three.
        //   a single-device rig is unaffected: its one device leaving IS the last one, so the
        // reset still happens on exactly the disconnects it always did.
        if(cf_usb_mounted_count() == 0)
                crossfire_usbhost_request_reset();
}

#endif // CF_HAVE_MIDI_BACKEND
