#include <string.h>

#include "crossfire_midi_backend.h"
#include "mock_midi.h"

struct mock_packet_queue {
        uint8_t packet[MOCK_MIDI_QUEUE_SIZE][4];
        uint32_t count;
};

static struct mock_packet_queue rx_queue[MOCK_MIDI_MAX_DEVICES]; // fed by test, drained by tuh_midi_packet_read()
static struct mock_packet_queue tx_queue[MOCK_MIDI_MAX_DEVICES]; // filled by tuh_midi_packet_write(), drained by test

//   bus position per USB device address, indexed by daddr. Sized past what tinyusb could hand
// out (TOTAL_DEVICES = CFG_TUH_DEVICE_MAX + CFG_TUH_HUB = 5, addresses 1..5) so a test can name
// a hub address without having to be careful. Zeroed by mock_midi_reset(), which means every
// address starts out root-attached
#define MOCK_MIDI_MAX_DADDR     8
static tuh_bus_info_t bus_info[MOCK_MIDI_MAX_DADDR];

static void queue_append(struct mock_packet_queue *q, const uint8_t packet[4])
{
        if(q->count >= MOCK_MIDI_QUEUE_SIZE)
                return;
        memcpy(q->packet[q->count], packet, 4);
        q->count++;
}

static bool queue_take(struct mock_packet_queue *q, uint8_t packet[4])
{
        if(q->count == 0)
                return false;
        memcpy(packet, q->packet[0], 4);
        memmove(q->packet[0], q->packet[1], (q->count - 1) * 4);
        q->count--;
        return true;
}

void mock_midi_reset(void)
{
        memset(rx_queue, 0, sizeof(rx_queue));
        memset(tx_queue, 0, sizeof(tx_queue));
        memset(bus_info, 0, sizeof(bus_info));
}

void mock_midi_set_bus_info(uint8_t daddr, uint8_t hub_addr, uint8_t hub_port)
{
        if(daddr >= MOCK_MIDI_MAX_DADDR)
                return;
        bus_info[daddr].rhport = 0;
        bus_info[daddr].hub_addr = hub_addr;
        bus_info[daddr].hub_port = hub_port;
        bus_info[daddr].speed = 1; // TUSB_SPEED_FULL; nothing under test reads it
}

void mock_midi_connect(uint8_t idx, uint8_t daddr, uint8_t rx_cables, uint8_t tx_cables)
{
        if(idx >= MOCK_MIDI_MAX_DEVICES)
                return;
        tuh_midi_mount_cb_t mount_cb_data = {
                .daddr = daddr,
                .bInterfaceNumber = 0,
                .rx_cable_count = rx_cables,
                .tx_cable_count = tx_cables,
        };
        tuh_midi_mount_cb(idx, &mount_cb_data);
}

void mock_midi_disconnect(uint8_t idx)
{
        if(idx >= MOCK_MIDI_MAX_DEVICES)
                return;
        tuh_midi_umount_cb(idx);
}

void mock_midi_feed(uint8_t idx, const uint8_t packet[4])
{
        if(idx >= MOCK_MIDI_MAX_DEVICES)
                return;
        queue_append(&rx_queue[idx], packet);
}

bool mock_midi_take_written(uint8_t idx, uint8_t packet[4])
{
        if(idx >= MOCK_MIDI_MAX_DEVICES)
                return false;
        return queue_take(&tx_queue[idx], packet);
}

// -- crossfire_midi_backend.h functions consumed by crossfire_forward.c --

uint32_t tuh_midi_packet_read_n(uint8_t idx, uint8_t *buffer, uint32_t bufsize)
{
        if(idx >= MOCK_MIDI_MAX_DEVICES || bufsize < 4)
                return 0;
        return queue_take(&rx_queue[idx], buffer) ? 4 : 0;
}

uint32_t tuh_midi_packet_write_n(uint8_t idx, const uint8_t *buffer, uint32_t bufsize)
{
        if(idx >= MOCK_MIDI_MAX_DEVICES || bufsize < 4)
                return 0;
        queue_append(&tx_queue[idx], buffer);
        return 4;
}

uint32_t tuh_midi_write_flush(uint8_t idx)
{
        (void)idx;
        return 0; // mock has no separate hardware-flush step; writes land immediately
}

bool tuh_bus_info_get(uint8_t daddr, tuh_bus_info_t *info)
{
        // tinyusb returns false for an address it knows nothing about, and crossfire_hub.c has
        // a path for that -- so the bound is part of what's being modelled, not just safety
        if(daddr >= MOCK_MIDI_MAX_DADDR)
                return false;
        *info = bus_info[daddr];
        return true;
}
