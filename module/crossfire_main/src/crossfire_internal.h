#ifndef _CROSSFIRE_INTERNAL_H
#define _CROSSFIRE_INTERNAL_H

#ifdef _HAVE_TINYUSB

#include <stdint.h>
#include <stdbool.h>

// simple built-in forwarding rule: incoming data on cable C of any mounted device is
// forwarded to cable C of every other mounted device, for whichever cable numbers both
// the source and that destination actually have. cables beyond this count on any one
// device are never used as forwarding sources or targets -- real USB-MIDI devices
// virtually always expose just one, so this is generous headroom, not a hard spec limit
#define CF_MAX_CABLES_PER_DEVICE       4

struct cf_midi_device {
        bool mounted;
        uint8_t daddr;
        uint8_t rx_cable_count;
        uint8_t tx_cable_count;
};

struct cf_forward_entry {
        uint8_t dst_idx;
        uint8_t dst_cable;
};

// forwarding targets for one (source device index, source cable) pair. CFG_TUH_MIDI - 1
// is the most targets a single source could ever have: every other mounted device
struct cf_forward_list {
        uint8_t count;
        struct cf_forward_entry entry[CFG_TUH_MIDI - 1];
};

extern struct cf_midi_device cf_midi_device[CFG_TUH_MIDI];
extern struct cf_forward_list cf_forward_table[CFG_TUH_MIDI][CF_MAX_CABLES_PER_DEVICE];

// recomputes cf_forward_table from the current cf_midi_device list, applying the
// built-in forwarding rule above. called whenever a device mounts or unmounts
extern void cf_forward_table_rebuild(void);

// drains any incoming MIDI stream data from every mounted device and forwards it
// according to cf_forward_table; called once per crossfire_task() tick
extern void cf_forward_service(void);

#endif // _HAVE_TINYUSB

#endif
