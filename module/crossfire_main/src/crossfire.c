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
        // which surface as tuh_midi_mount_cb()/tuh_midi_umount_cb() calls below); this
        // does not block, since osal_queue_receive() ignores its timeout under OPT_OS_NONE
        tuh_task();
        cf_forward_service();
#endif
}

#ifdef _HAVE_TINYUSB

#define CF_STREAM_BUF_SIZE     64

struct cf_midi_device cf_midi_device[CFG_TUH_MIDI];
struct cf_forward_list cf_forward_table[CFG_TUH_MIDI][CF_MAX_CABLES_PER_DEVICE];

void cf_forward_table_rebuild(void)
{
        for(uint8_t src_idx = 0; src_idx < CFG_TUH_MIDI; src_idx++) {
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
                        for(uint8_t dst_idx = 0; dst_idx < CFG_TUH_MIDI; dst_idx++) {
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
        bool wrote_any[CFG_TUH_MIDI] = { 0 };

        for(uint8_t src_idx = 0; src_idx < CFG_TUH_MIDI; src_idx++) {
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
        for(uint8_t idx = 0; idx < CFG_TUH_MIDI; idx++) {
                if(wrote_any[idx])
                        tuh_midi_write_flush(idx);
        }
}

void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t *mount_cb_data)
{
        if(idx >= CFG_TUH_MIDI)
                return;
        cf_midi_device[idx].mounted = true;
        cf_midi_device[idx].daddr = mount_cb_data->daddr;
        cf_midi_device[idx].rx_cable_count = mount_cb_data->rx_cable_count;
        cf_midi_device[idx].tx_cable_count = mount_cb_data->tx_cable_count;
        light_info("USB-MIDI device mounted: idx=%d daddr=%d rx_cables=%d tx_cables=%d",
                        idx, mount_cb_data->daddr, mount_cb_data->rx_cable_count, mount_cb_data->tx_cable_count);
        cf_forward_table_rebuild();
}
void tuh_midi_umount_cb(uint8_t idx)
{
        if(idx >= CFG_TUH_MIDI)
                return;
        light_info("USB-MIDI device unmounted: idx=%d daddr=%d", idx, cf_midi_device[idx].daddr);
        cf_midi_device[idx].mounted = false;
        cf_forward_table_rebuild();
}

#endif // _HAVE_TINYUSB
