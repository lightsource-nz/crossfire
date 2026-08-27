#ifdef CF_HAVE_USB_HUB

#include <light.h>

#include "crossfire_internal.h"
#include "crossfire_midi_backend.h"

//   HUB MODE. The custom crossfire PCB reaches four instruments through four independent
// PIO-emulated host ports; this mode reaches the same four through ONE hardware root port and
// a downstream USB hub, which is what a stock Pico (or the H7 board) can actually do today.
// Nothing else about the firmware changes: the forwarding engine already broadcasts across
// every mounted device slot regardless of how the device got there.
//
//   what this file adds is position. tinyusb hands the application a mount index, which is a
// slot in its own MIDI-interface table and says nothing about which socket the cable is in --
// it is not even stable across a reconnect, since the freed slot goes to whichever device
// enumerates next. The hub's own downstream port number is the stable, physical identity, and
// tuh_bus_info_get() is the one place tinyusb exposes it.
//
//   nothing here participates in enumeration or transfers. tinyusb's hub class driver
// (host/hub.c, compiled in whenever CFG_TUH_HUB is set -- which crossfire's tusb_config.h has
// always done) does all of that on its own.

// USB address of the hub, learned from the first device that mounts behind one. 0 until then.
//   there is no "hub mounted" callback to hook: usbh.c invokes tuh_mount_cb() for every
// enumerated device EXCEPT hubs, for which it logs and returns. So an empty hub is
// indistinguishable from no hub at all from up here, and that is fine -- an empty hub has
// nothing to forward.
static uint8_t cf_hub_device_addr;

// hub port number (1-based, as the hub numbers its own ports) for each real device slot, or
// CF_HUB_PORT_NONE for a slot that is unmounted or attached straight to the root port.
// indexed by the same tinyusb mount index the forwarding engine uses
static uint8_t cf_device_hub_port[CF_MAX_DEVICES_USB];

uint8_t cf_hub_addr(void)
{
        return cf_hub_device_addr;
}

uint8_t cf_hub_port_of_device(uint8_t idx)
{
        if(idx >= CF_MAX_DEVICES_USB)
                return CF_HUB_PORT_NONE;
        return cf_device_hub_port[idx];
}

bool cf_hub_port_occupied(uint8_t port)
{
        if(port == CF_HUB_PORT_NONE)
                return false;
        for(uint8_t idx = 0; idx < CF_MAX_DEVICES_USB; idx++) {
                if(cf_midi_device[idx].mounted && cf_device_hub_port[idx] == port)
                        return true;
        }
        return false;
}

void cf_hub_device_attached(uint8_t idx, uint8_t daddr)
{
        if(idx >= CF_MAX_DEVICES_USB)
                return;

        tuh_bus_info_t bus;
        if(!tuh_bus_info_get(daddr, &bus)) {
                //   not a failure worth refusing the device over: the device is mounted and
                // will forward MIDI either way, we just cannot say where it is plugged in
                light_warn("hub: no bus info for daddr=%d; treating idx=%d as root-attached", daddr, idx);
                cf_device_hub_port[idx] = CF_HUB_PORT_NONE;
                return;
        }

        if(bus.hub_addr == 0) {
                // straight into the root port -- the single-device topology, still perfectly
                // usable in a hub build (and what you get before the hub is plugged in)
                cf_device_hub_port[idx] = CF_HUB_PORT_NONE;
                light_info("USB-MIDI device idx=%d is attached directly to the root port", idx);
                return;
        }

        //   CFG_TUH_HUB is 1, so tinyusb enumerates exactly one hub and every hub_addr seen
        // here is that same hub. Logged rather than handled if that ever stops being true,
        // because the port numbers of two different hubs would otherwise silently collide in
        // cf_hub_port_occupied()
        if(cf_hub_device_addr != 0 && cf_hub_device_addr != bus.hub_addr) {
                light_warn("hub: device idx=%d is behind hub addr=%d, but hub addr=%d was already known;"
                                " port numbers from the two will be conflated",
                                idx, bus.hub_addr, cf_hub_device_addr);
        } else if(cf_hub_device_addr == 0) {
                light_info("USB hub detected at address %d", bus.hub_addr);
        }
        cf_hub_device_addr = bus.hub_addr;

        cf_device_hub_port[idx] = bus.hub_port;
        //   a device on a port above CF_MAX_DEVICES_USB is kept and forwarded like any other;
        // the cap is on how many MIDI interfaces tinyusb tracks at once (CFG_TUH_MIDI), not on
        // which port numbers a hub is allowed to report. It just has no cell of its own on the
        // status display, which only has room for the first four
        if(bus.hub_port > CF_MAX_DEVICES_USB) {
                light_info("USB-MIDI device idx=%d is on hub port %d, beyond the %d ports shown on the display",
                                idx, bus.hub_port, CF_MAX_DEVICES_USB);
        } else {
                light_info("USB-MIDI device idx=%d is on hub port %d", idx, bus.hub_port);
        }
}

void cf_hub_device_detached(uint8_t idx)
{
        if(idx >= CF_MAX_DEVICES_USB)
                return;
        cf_device_hub_port[idx] = CF_HUB_PORT_NONE;

        //   the hub address is forgotten only once nothing is left behind it, so that pulling
        // one instrument does not make the display claim the hub went away. Unplugging the hub
        // itself unmounts every device below it, so this does fire then
        if(cf_usb_mounted_count() == 0)
                cf_hub_device_addr = 0;
}

#endif // CF_HAVE_USB_HUB
