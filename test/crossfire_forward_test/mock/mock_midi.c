#include <string.h>

#include "crossfire_midi_backend.h"
#include "mock_midi.h"

struct mock_queue {
        uint8_t data[MOCK_MIDI_QUEUE_SIZE];
        uint32_t len;
};

static struct mock_queue rx_queue[MOCK_MIDI_MAX_DEVICES][MOCK_MIDI_MAX_CABLES]; // fed by test, drained by tuh_midi_stream_read()
static struct mock_queue tx_queue[MOCK_MIDI_MAX_DEVICES][MOCK_MIDI_MAX_CABLES]; // filled by tuh_midi_stream_write(), drained by test
static uint8_t rx_cursor[MOCK_MIDI_MAX_DEVICES]; // next cable to check in tuh_midi_stream_read()'s round-robin scan

static void queue_append(struct mock_queue *q, const uint8_t *data, uint32_t len)
{
        if(len > MOCK_MIDI_QUEUE_SIZE - q->len)
                len = MOCK_MIDI_QUEUE_SIZE - q->len;
        memcpy(q->data + q->len, data, len);
        q->len += len;
}

static uint32_t queue_take(struct mock_queue *q, uint8_t *out, uint32_t out_size)
{
        uint32_t n = q->len < out_size ? q->len : out_size;
        memcpy(out, q->data, n);
        memmove(q->data, q->data + n, q->len - n);
        q->len -= n;
        return n;
}

void mock_midi_reset(void)
{
        memset(rx_queue, 0, sizeof(rx_queue));
        memset(tx_queue, 0, sizeof(tx_queue));
        memset(rx_cursor, 0, sizeof(rx_cursor));
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

void mock_midi_feed(uint8_t idx, uint8_t cable, const uint8_t *data, uint32_t len)
{
        if(idx >= MOCK_MIDI_MAX_DEVICES || cable >= MOCK_MIDI_MAX_CABLES)
                return;
        queue_append(&rx_queue[idx][cable], data, len);
}

uint32_t mock_midi_take_written(uint8_t idx, uint8_t cable, uint8_t *out, uint32_t out_size)
{
        if(idx >= MOCK_MIDI_MAX_DEVICES || cable >= MOCK_MIDI_MAX_CABLES)
                return 0;
        return queue_take(&tx_queue[idx][cable], out, out_size);
}

// -- crossfire_midi_backend.h functions consumed by crossfire_forward.c --

uint32_t tuh_midi_stream_read(uint8_t idx, uint8_t *p_cable_num, uint8_t *p_buffer, uint16_t bufsize)
{
        if(idx >= MOCK_MIDI_MAX_DEVICES)
                return 0;
        // real tinyusb stops at the first cable-number change within one call; here we
        // just drain a whole cable's queue per call, which satisfies the same "loop
        // until 0" contract the caller relies on without needing to emulate interleaving
        for(uint8_t i = 0; i < MOCK_MIDI_MAX_CABLES; i++) {
                uint8_t cable = (rx_cursor[idx] + i) % MOCK_MIDI_MAX_CABLES;
                struct mock_queue *q = &rx_queue[idx][cable];
                if(q->len == 0)
                        continue;
                rx_cursor[idx] = (cable + 1) % MOCK_MIDI_MAX_CABLES;
                *p_cable_num = cable;
                return queue_take(q, p_buffer, bufsize);
        }
        return 0;
}

uint32_t tuh_midi_stream_write(uint8_t idx, uint8_t cable_num, const uint8_t *p_buffer, uint32_t bufsize)
{
        if(idx >= MOCK_MIDI_MAX_DEVICES || cable_num >= MOCK_MIDI_MAX_CABLES)
                return 0;
        queue_append(&tx_queue[idx][cable_num], p_buffer, bufsize);
        return bufsize;
}

uint32_t tuh_midi_write_flush(uint8_t idx)
{
        (void)idx;
        return 0; // mock has no separate hardware-flush step; writes land immediately
}
