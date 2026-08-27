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

// max number of real (tinyusb-backed) MIDI devices tracked at once. on target builds this
// must stay <= tusb_config.h's CFG_TUH_MIDI, since tinyusb never hands out a mount idx
// beyond that
#ifndef CF_MAX_DEVICES_USB
#define CF_MAX_DEVICES_USB             4
#endif

#ifdef CF_HAVE_SPI_LINK
// one extra slot, index CF_MAX_DEVICES_USB, reserved exclusively for the SPI-linked peer
// board (see crossfire_spi_link.c) -- tinyusb's own idx space tops out at
// CF_MAX_DEVICES_USB-1, so this can never collide with a real mount callback
#define CF_LINK_DEVICE_IDX             CF_MAX_DEVICES_USB
#define CF_MAX_DEVICES                 (CF_MAX_DEVICES_USB + 1)
#else
#define CF_MAX_DEVICES                 CF_MAX_DEVICES_USB
#endif

enum cf_device_kind {
        CF_DEVICE_KIND_USB = 0,
        CF_DEVICE_KIND_SPI_LINK
};

struct cf_midi_device {
        bool mounted;
        enum cf_device_kind kind;
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

// how many of the real (tinyusb-backed) device slots are currently mounted. the SPI link's
// reserved slot is never counted -- it is not on the USB bus and is mounted for the life of
// the program. used to decide whether a disconnect emptied the root port
// (crossfire_usbhost_request_reset()) and to drive the device count on the status display
extern uint8_t cf_usb_mounted_count(void);

#ifdef CF_HAVE_USB_HUB
// HUB MODE: one USB hub on the root port, with up to CF_MAX_DEVICES_USB MIDI devices on its
// downstream ports, instead of one device per root port. The forwarding engine itself is
// unchanged by this -- it already broadcasts across every mounted slot -- so everything here
// is about knowing WHERE a device is: which hub, which port. See crossfire_hub.c.
//
//   the port numbers are the hub's own 1-based downstream port numbers, straight from
// tuh_bus_info_get(). A hub with more than CF_MAX_DEVICES_USB ports reports ports above that
// range and those devices still mount and still forward -- CF_MAX_DEVICES_USB caps how many
// MIDI interfaces tinyusb will track (CFG_TUH_MIDI), not which port numbers are legal
#define CF_HUB_PORT_NONE               0

// records where a newly-mounted device sits, from its USB address. called from
// tuh_midi_mount_cb() before the forwarding table is rebuilt
extern void cf_hub_device_attached(uint8_t idx, uint8_t daddr);
// forgets a device's position; called from tuh_midi_umount_cb()
extern void cf_hub_device_detached(uint8_t idx);
// the hub port 'idx' is attached to, or CF_HUB_PORT_NONE if it is on the root port directly
// (or not mounted at all)
extern uint8_t cf_hub_port_of_device(uint8_t idx);
// USB address of the hub devices have been seen behind, or 0 if none has been seen yet.
// there is no callback for a hub mounting, so this only becomes non-zero once the first
// MIDI device behind it enumerates -- see crossfire_midi_backend.h
extern uint8_t cf_hub_addr(void);
// true if a mounted MIDI device currently occupies this hub port. 'port' is 1-based, as the
// hub numbers its own ports; port 0 (CF_HUB_PORT_NONE) is never occupied
extern bool cf_hub_port_occupied(uint8_t port);
#endif // CF_HAVE_USB_HUB

// how long the RX/TX status indicators stay lit after the most recent activity, before
// cf_activity_indicators_service() turns them off again
#define CF_ACTIVITY_INDICATOR_MS       150

// true if a MIDI message was received from / forwarded to any device within the last
// CF_ACTIVITY_INDICATOR_MS. queried by crossfire_display_update_status() to decide
// whether to draw the RX/TX indicators
extern bool cf_rx_indicator_active(void);
extern bool cf_tx_indicator_active(void);
// checks whether either indicator's active window has just expired (or just started) and,
// if the on-screen state actually needs to change, redraws the status display. must be
// called once per crossfire_task() tick -- indicators are only ever turned on in response
// to real traffic (cf_forward_service()), but turning them off again after
// CF_ACTIVITY_INDICATOR_MS needs something polling the clock even when nothing new arrives
extern void cf_activity_indicators_service(void);

#ifdef CF_HAVE_SPI_LINK
// marks the SPI-linked peer's reserved forwarding-table slot (CF_LINK_DEVICE_IDX) as
// mounted and rebuilds cf_forward_table -- there's no discovery/handshake with the actual
// peer board, this just makes the slot a forwarding participant unconditionally once
// compiled in. called once from crossfire_init(), defined in crossfire_forward.c
extern void cf_link_device_mount(void);
// brings up both SPI peripherals backing the inter-board link (master/TX for the outgoing
// direction, slave/RX for the incoming one) -- see crossfire_spi_link.c for pin
// assignments. called once from crossfire_init()
extern void cf_spi_link_init(void);
// non-blocking: drains whatever's arrived so far on the RX link into a buffer that
// persists across calls, returning true (and filling 'packet') only once a full 4-byte
// USB-MIDI Event Packet has been received. call every crossfire_task() tick
extern bool cf_spi_link_packet_read(uint8_t packet[4]);
// blocking burst (CS low, write 4 bytes, CS high) -- negligible duration for 4 bytes at
// any reasonable SPI clock, so unlike the display driver's async DMA path this doesn't
// need to be non-blocking
extern void cf_spi_link_packet_write(const uint8_t packet[4]);
#endif // CF_HAVE_SPI_LINK

#endif // CF_HAVE_MIDI_BACKEND

// redraws the status display (currently just the count of mounted MIDI devices) from
// whatever state crossfire_forward.c last computed; called after every mount/unmount so
// the screen never shows stale device counts. pushes the whole panel
extern void crossfire_display_update_status(void);
// same redraw, but only pushes the RX/TX indicator band to the panel. for the very
// frequent case where nothing but an indicator changed -- see the definition for why
// limiting the pushed region is what stops them visibly wiping across the display.
// the text is left as-is on the panel, so this must not be used when it could have
// changed (use crossfire_display_update_status() then)
extern void crossfire_display_update_indicators(void);

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
// enumerate.
//
//   resetting the whole controller is coarse: it drops every other device sharing this root
// port too. That was harmless while exactly one device could ever be mounted, and is not
// once a hub is in front of the port -- unplugging one instrument would silently take the
// other three down with it. So the request is now made only when a disconnect leaves NO USB
// device mounted at all (see tuh_midi_umount_cb()), which is precisely the root-port-empty
// case the workaround was written for. A single-device rig behaves exactly as before,
// because there the departing device is always the last one.
extern void crossfire_usbhost_request_reset(void);

#endif
