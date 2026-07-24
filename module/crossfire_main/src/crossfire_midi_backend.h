#ifndef _CROSSFIRE_MIDI_BACKEND_H
#define _CROSSFIRE_MIDI_BACKEND_H

#include <stdint.h>

// Minimal slice of tinyusb's USB-MIDI host API (src/class/midi/midi_host.h) that
// the forwarding engine (crossfire_forward.c) actually touches. Declared directly
// here instead of pulling in the real tusb.h, so the engine can be linked against
// either the real tinyusb host stack (target builds) or a mock backend (host test
// builds, see test/crossfire_forward_test/mock) without any changes.
//
// Struct layout and function signatures must be kept in sync with tinyusb's.

typedef struct {
        uint8_t daddr;
        uint8_t bInterfaceNumber;
        uint8_t rx_cable_count;
        uint8_t tx_cable_count;
} tuh_midi_mount_cb_t;

// host stack -> app queries, implemented by whichever backend is linked in
extern uint32_t tuh_midi_stream_write(uint8_t idx, uint8_t cable_num, const uint8_t *p_buffer, uint32_t bufsize);
extern uint32_t tuh_midi_stream_read(uint8_t idx, uint8_t *p_cable_num, uint8_t *p_buffer, uint16_t bufsize);
extern uint32_t tuh_midi_write_flush(uint8_t idx);

// app callbacks, implemented in crossfire_forward.c and invoked by whichever
// backend is linked in (tinyusb itself on target builds, mock_midi.c in tests)
extern void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t *mount_cb_data);
extern void tuh_midi_umount_cb(uint8_t idx);

#endif
