#ifndef _CROSSFIRE_INTERNAL_H
#define _CROSSFIRE_INTERNAL_H

#ifdef CF_HAVE_MIDI_BACKEND

#include <stdint.h>
#include <stdbool.h>

// simple built-in forwarding rule: incoming data on cable C of any mounted device is
// forwarded to cable C of every other mounted device, for whichever cable numbers both
// the source and that destination actually have. cables beyond this count on any one
// device are never used as forwarding sources or targets -- real USB-MIDI devices
// virtually always expose just one, so this is generous headroom, not a hard spec limit
#define CF_MAX_CABLES_PER_DEVICE       4

// max number of MIDI devices tracked at once. on target builds this must stay <=
// tusb_config.h's CFG_TUH_MIDI, since tinyusb never hands out a mount idx beyond that
#ifndef CF_MAX_DEVICES
#define CF_MAX_DEVICES                 4
#endif

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

// forwarding targets for one (source device index, source cable) pair. CF_MAX_DEVICES - 1
// is the most targets a single source could ever have: every other mounted device
struct cf_forward_list {
        uint8_t count;
        struct cf_forward_entry entry[CF_MAX_DEVICES - 1];
};

extern struct cf_midi_device cf_midi_device[CF_MAX_DEVICES];
extern struct cf_forward_list cf_forward_table[CF_MAX_DEVICES][CF_MAX_CABLES_PER_DEVICE];

// recomputes cf_forward_table from the current cf_midi_device list, applying the
// built-in forwarding rule above. called whenever a device mounts or unmounts
extern void cf_forward_table_rebuild(void);

// drains any incoming MIDI stream data from every mounted device and forwards it
// according to cf_forward_table; called once per crossfire_task() tick
extern void cf_forward_service(void);

#endif // CF_HAVE_MIDI_BACKEND

// redraws the status display (currently just the count of mounted MIDI devices) from
// whatever state crossfire_forward.c last computed; called after every mount/unmount so
// the screen never shows stale device counts
extern void crossfire_display_update_status(void);

// requests a full USB host controller teardown+reinit on the next crossfire_task() tick.
// deferred rather than performed inline from tuh_midi_umount_cb() -- tearing down and
// reinitializing the host stack from inside a callback tinyusb itself is still unwinding
// for the same disconnect event is not safe (see TinyUSB's own dual/dynamic_switch example,
// which does the same teardown/delay/reinit sequence from its main loop, never from a
// mount/umount callback).
//
// why this exists: RP2040's native USB host controller can leave stale hardware
// buffer-control state behind across a disconnect (an acknowledged upstream tinyusb/RP2040
// issue -- see hathach/tinyusb#3533 -- not something fixable from application code alone),
// which panics ("buf_ctrl ... already available") the next time a device tries to
// enumerate. resetting the whole controller after every disconnect is a coarse fix -- it
// also drops any OTHER currently-mounted device sharing this root port, which is fine for
// today's single-device test setup but will need revisiting once multiple simultaneous
// devices/hub ports are in play
extern void crossfire_usbhost_request_reset(void);

#endif
