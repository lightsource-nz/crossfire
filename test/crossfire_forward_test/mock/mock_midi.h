#ifndef _MOCK_MIDI_H
#define _MOCK_MIDI_H

#include <stdint.h>

// A mock USB-MIDI host backend implementing the crossfire_midi_backend.h seam.
// Stands in for tinyusb's real tuh_midi_* API so crossfire's forwarding engine
// (crossfire_forward.c) can be exercised against fake devices and predefined
// message streams, without any real USB hardware or tinyusb involved.

#define MOCK_MIDI_MAX_DEVICES   4
#define MOCK_MIDI_MAX_CABLES    4
#define MOCK_MIDI_QUEUE_SIZE    256

// clears all simulated devices and queued data
extern void mock_midi_reset(void);

// simulates a device mounting; invokes tuh_midi_mount_cb() exactly as tinyusb would
extern void mock_midi_connect(uint8_t idx, uint8_t daddr, uint8_t rx_cables, uint8_t tx_cables);

// simulates a device unmounting; invokes tuh_midi_umount_cb()
extern void mock_midi_disconnect(uint8_t idx);

// queues bytes as if received from the device on the given cable; drained by
// crossfire's forwarding engine via tuh_midi_stream_read()
extern void mock_midi_feed(uint8_t idx, uint8_t cable, const uint8_t *data, uint32_t len);

// drains bytes that were written to a device's outgoing cable via
// tuh_midi_stream_write(), so a test can inspect what crossfire actually forwarded
extern uint32_t mock_midi_take_written(uint8_t idx, uint8_t cable, uint8_t *out, uint32_t out_size);

#endif
