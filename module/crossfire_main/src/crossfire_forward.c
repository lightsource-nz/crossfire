#ifdef CF_HAVE_MIDI_BACKEND

#include <light.h>

#include <hardware/gpio.h>

#include "crossfire_internal.h"
#include "crossfire_midi_backend.h"

#define CF_STREAM_BUF_SIZE     64
#define CF_MIDI_LED_PIN        25

struct cf_midi_device cf_midi_device[CF_MAX_DEVICES];
struct cf_forward_list cf_forward_table[CF_MAX_DEVICES][CF_MAX_CABLES_PER_DEVICE];

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

void cf_forward_service(void)
{
        uint8_t buffer[CF_STREAM_BUF_SIZE];
        bool wrote_any[CF_MAX_DEVICES] = { 0 };

        for(uint8_t src_idx = 0; src_idx < CF_MAX_DEVICES; src_idx++) {
                if(!cf_midi_device[src_idx].mounted)
                        continue;

                // tuh_midi_stream_read() stops at the first cable-number change, so it must
                // be called in a loop to fully drain the incoming FIFO on every tick
                uint8_t cable_num;
                uint32_t n;
                while((n = tuh_midi_stream_read(src_idx, &cable_num, buffer, sizeof(buffer))) > 0) {
                        if(cable_num >= CF_MAX_CABLES_PER_DEVICE)
                                continue;
                        struct cf_forward_list *list = &cf_forward_table[src_idx][cable_num];
                        for(uint8_t i = 0; i < list->count; i++) {
                                struct cf_forward_entry *fwd = &list->entry[i];
                                tuh_midi_stream_write(fwd->dst_idx, fwd->dst_cable, buffer, n);
                                wrote_any[fwd->dst_idx] = true;
                        }
                }
        }
        for(uint8_t idx = 0; idx < CF_MAX_DEVICES; idx++) {
                if(wrote_any[idx])
                        tuh_midi_write_flush(idx);
        }
}

void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t *mount_cb_data)
{
        if(idx >= CF_MAX_DEVICES)
                return;
        cf_midi_device[idx].mounted = true;
        cf_midi_device[idx].daddr = mount_cb_data->daddr;
        cf_midi_device[idx].rx_cable_count = mount_cb_data->rx_cable_count;
        cf_midi_device[idx].tx_cable_count = mount_cb_data->tx_cable_count;
        light_info("USB-MIDI device mounted: idx=%d daddr=%d rx_cables=%d tx_cables=%d",
                        idx, mount_cb_data->daddr, mount_cb_data->rx_cable_count, mount_cb_data->tx_cable_count);
        cf_forward_table_rebuild();
        gpio_put(CF_MIDI_LED_PIN, true);
        crossfire_display_update_status();
}
void tuh_midi_umount_cb(uint8_t idx)
{
        if(idx >= CF_MAX_DEVICES)
                return;
        light_info("USB-MIDI device unmounted: idx=%d daddr=%d", idx, cf_midi_device[idx].daddr);
        cf_midi_device[idx].mounted = false;
        cf_forward_table_rebuild();
        gpio_put(CF_MIDI_LED_PIN, false);
        crossfire_display_update_status();
}

#endif // CF_HAVE_MIDI_BACKEND
