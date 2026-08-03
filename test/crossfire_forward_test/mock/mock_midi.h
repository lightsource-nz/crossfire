#ifndef _MOCK_MIDI_H
#define _MOCK_MIDI_H

#include <stdint.h>
#include <stdbool.h>

// A mock USB-MIDI host backend implementing the crossfire_midi_backend.h seam.
// Stands in for tinyusb's real tuh_midi_* API so crossfire's forwarding engine
// (crossfire_forward.c) can be exercised against fake devices and predefined
// message streams, without any real USB hardware or tinyusb involved.

#define MOCK_MIDI_MAX_DEVICES   4
#define MOCK_MIDI_MAX_CABLES    4
#define MOCK_MIDI_QUEUE_SIZE    64 // packets, not bytes -- one device's worth of headroom

// clears all simulated devices and queued data
extern void mock_midi_reset(void);

// simulates a device mounting; invokes tuh_midi_mount_cb() exactly as tinyusb would
extern void mock_midi_connect(uint8_t idx, uint8_t daddr, uint8_t rx_cables, uint8_t tx_cables);

// simulates a device unmounting; invokes tuh_midi_umount_cb()
extern void mock_midi_disconnect(uint8_t idx);

// queues one raw 4-byte USB-MIDI Event Packet as if received from the device (cable
// number is whatever's embedded in packet[0]'s high nibble) -- drained by crossfire's
// forwarding engine via tuh_midi_packet_read()
extern void mock_midi_feed(uint8_t idx, const uint8_t packet[4]);

// takes the next packet written to a device via tuh_midi_packet_write(), so a test can
// inspect what crossfire actually forwarded. returns false if nothing was written
extern bool mock_midi_take_written(uint8_t idx, uint8_t packet[4]);

#endif
