#include <light_usb.h>

#ifdef __HAVE_TINYUSB
#include "bsp/board.h"
#include "tusb.h"
#endif

#ifdef __HAVE_RP2_HW
#define LIGHT_USB_NATIVE_PORT_COUNT 1
#define LIGHT_USB_PIO_ENABLED 1
#define LIGHT_USB_PIO_SM_MIN 0
#define LIGHT_USB_PIO_SM_MAX 7
#endif

static struct light_usb_port _native_port[LIGHT_USB_NATIVE_PORT_COUNT];

void light_usb_init()
{
#ifdef __HAVE_TINYUSB
        board_init();
#endif
        for(uint8_t i = 0; i < LIGHT_USB_NATIVE_PORT_COUNT; i++) {

        }
}

uint8_t light_usb_get_native_port_count()
{
#ifdef __RP2040
        return 1;
#endif
}
struct light_usb_port *light_usb_get_native_port(uint8_t phys_id);
bool light_usb_pio_driver_available();
struct light_usb_port *light_usb_create_pio_port(struct light_usb_pio_port_desc desc);

uint8_t light_usb_get_port_status(struct light_usb_port *port);
bool light_usb_port_has_capability(struct light_usb_port *port, uint8_t capability);
bool light_usb_claim_port(uint8_t port_id, struct light_module *owner);
void light_usb_release_port(struct light_usb_port *port);
bool light_usb_activate_port(struct light_usb_port *port, uint8_t port_mode);
void light_usb_deactivate_port(struct light_usb_port *port)
{

}
