#ifndef _LIGHT_USB_H
#define _LIGHT_USB_H

#include <light.h>

enum light_usb_port_type {
        LIGHT_USB_PORT_NATIVE = 0,
        LIGHT_USB_PORT_PIO
};
enum light_usb_port_status {
        LIGHT_USB_PORT_INIT = 0,
        LIGHT_USB_PORT_CLAIMED,
        LIGHT_USB_PORT_ACTIVE
};
enum light_usb_port_mode {
        LIGHT_USB_PORT_MODE_DEVICE = 0,
        LIGHT_USB_PORT_MODE_HOST,
        LIGHT_USB_PORT_MODE_OTG
};

#define LIGHT_USB_PORT_CAPS_DEVICE              0
#define LIGHT_USB_PORT_CAPS_HOST                1
#define LIGHT_USB_PORT_CAPS_OTG                 2

// device-mode ports need only data +/- pins
// host-mode and otg ports also need a vbe pin
// otg ports additionally require an otg_id pin
struct light_usb_pio_port_desc {
        uint8_t pin_data_minus;
        uint8_t pin_data_plus;
        uint8_t pin_vbus_enable;
        uint8_t pin_otg_id;
};
struct light_usb_port {
        uint8_t port_id;        // globally unique port identifier
        uint8_t phys_id;        // physical port or pio instance number
        enum light_usb_port_type port_type;
        volatile struct light_module *owner;
        volatile enum light_usb_port_status status;
        enum light_usb_port_mode mode;
};

extern void light_usb_init();

extern uint8_t light_usb_get_native_port_count();
extern struct light_usb_port *light_usb_get_native_port(uint8_t phys_id);
extern bool light_usb_pio_driver_available();
extern struct light_usb_port *light_usb_create_pio_port(struct light_usb_pio_port_desc desc);

extern uint8_t light_usb_get_port_status(struct light_usb_port *port);
extern bool light_usb_port_has_capability(struct light_usb_port *port, uint8_t capability);
static inline bool light_usb_port_can_be_device(struct light_usb_port *port)
{
        return light_usb_port_has_capability(port, LIGHT_USB_PORT_CAPS_DEVICE);
}
static inline bool light_usb_port_can_be_host(struct light_usb_port *port)
{
        return light_usb_port_has_capability(port, LIGHT_USB_PORT_CAPS_HOST);
}
static inline bool light_usb_port_can_be_otg(struct light_usb_port *port)
{
        return light_usb_port_has_capability(port, LIGHT_USB_PORT_CAPS_OTG);
}
extern bool light_usb_claim_port(uint8_t port_id, struct light_module *owner);
extern void light_usb_release_port(struct light_usb_port *port);
extern bool light_usb_activate_port(struct light_usb_port *port, uint8_t port_mode);
extern void light_usb_deactivate_port(struct light_usb_port *port);

#endif