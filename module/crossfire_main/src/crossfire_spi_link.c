#ifdef CF_HAVE_SPI_LINK

#include <light_ioport.h>

#include "crossfire_internal.h"

// two independent one-way links rather than a single shared bus -- each direction has its
// own master, so there's no arbitration between the two boards to design around. CS
// delimits each 4-byte USB-MIDI Event Packet as its own transaction, so the receiving side
// never needs a custom framing/resync scheme on top of what the hardware already gives for
// free. pin numbers are a starting point, not fixed by the protocol -- adjust freely to
// match actual wiring, just keep OUT_PIN_CS on the same peripheral's canonical pin group as
// its SCK/MOSI (and likewise for IN), since RP2040 SPI pins aren't freely mixable across
// the peripheral's alternate pin groups

//   THROUGH light_ioport, not the Pico SDK directly. This used spi_init()/gpio_put()/
// spi_get_hw()->dr, which pinned the whole feature -- and therefore crossfire -- to RP2. The
// transport abstraction already existed for displays; what it lacked was a receiving role, so
// IO_SPI_SLAVE was added rather than this file keeping its own private SPI driver.
//
//   PIN NUMBERS ARE STILL PLATFORM-SPECIFIC and always will be: these are flat RP2 GPIO
// numbers. A port to another part supplies its own, written with LIGHT_IOPORT_PIN_STM32() on
// STM32. What is portable is the transport, not the wiring.

// OUT link: this board is master, drives the peer's IN link
#define CF_LINK_OUT_PORT        PORT_SPI_0
#define CF_LINK_OUT_PIN_SCK     18
#define CF_LINK_OUT_PIN_MOSI    19
#define CF_LINK_OUT_PIN_CS      17

// IN link: this board is slave, driven by the peer's OUT link. spi1 has two alternate
// pin groups (GP8-11 and GP12-15) -- this uses the GP12-15 one specifically because
// GP8-11 (SCK=10, MOSI=11, CS=9) exactly matches the PO13 status display's own pins
// (light_display_po13.h). display init is compiled out entirely whenever this link is
// enabled, so that overlap isn't a live runtime conflict today, but there's no reason to
// leave the coincidence in place when a non-overlapping pin group is just as available
#define CF_LINK_IN_PORT         PORT_SPI_1
#define CF_LINK_IN_PIN_SCK      14
#define CF_LINK_IN_PIN_MOSI     15
#define CF_LINK_IN_PIN_CS       13

// 4 bytes at any reasonable clock is sub-microsecond -- this just needs to be slow enough
// for both boards' wiring to be reliable on a breadboard, not tuned for throughput
#define CF_LINK_BAUDRATE        (1 * 1000 * 1000)

// accumulates across cf_spi_link_packet_read() calls -- a whole 4-byte burst will
// virtually always complete within one scheduler tick, but this doesn't assume that
static uint8_t rx_packet_buf[4];
static uint8_t rx_packet_len;

static struct io_context *link_out;
static struct io_context *link_in;

void cf_spi_link_init(void)
{
        //   the 3-pin master: SCK, MOSI, CS and no D/C, which is the display convention this
        // link has no use for. light_ioport asserts CS around each burst, which is exactly the
        // per-packet framing the receiving side relies on -- previously done by hand here.
        //   LIGHT_IOPORT_PIN_NONE for reset: the far end is a peer, not a device to reset.
        link_out = light_ioport_setup_io_spi_3p(CF_LINK_OUT_PORT, LIGHT_IOPORT_PIN_NONE,
                                        CF_LINK_OUT_PIN_CS, CF_LINK_OUT_PIN_SCK, CF_LINK_OUT_PIN_MOSI);
        light_ioport_set_spi_clock(link_out, CF_LINK_BAUDRATE);

        // in slave mode the hardware itself uses the CS *input* to know when a
        // transaction starts/ends, so unlike the OUT link, CS here does need to be muxed
        // to the peripheral rather than driven by software
        link_in = light_ioport_setup_io_spi_slave(CF_LINK_IN_PORT,
                                        CF_LINK_IN_PIN_CS, CF_LINK_IN_PIN_SCK, CF_LINK_IN_PIN_MOSI);
        light_ioport_set_spi_clock(link_in, CF_LINK_BAUDRATE);

        rx_packet_len = 0;
}

bool cf_spi_link_packet_read(uint8_t packet[4])
{
        //   read_available() drains what the peripheral already holds and returns immediately,
        // so this keeps its accumulate-across-calls shape: a slave has no say in when its master
        // clocks bytes, and blocking a scheduler tick on a peer that may say nothing is exactly
        // what this loop was written to avoid.
        rx_packet_len += (uint8_t) light_ioport_read_available(link_in,
                                        &rx_packet_buf[rx_packet_len], 4u - rx_packet_len);
        if(rx_packet_len < 4)
                return false;

        for(uint8_t i = 0; i < 4; i++)
                packet[i] = rx_packet_buf[i];
        rx_packet_len = 0;
        return true;
}

void cf_spi_link_packet_write(const uint8_t packet[4])
{
        // one burst under one CS assertion, which light_ioport does itself -- the manual
        // gpio_put() pair around the transfer is now the transport's business, not this file's
        light_ioport_send_data_burst(link_out, packet, 4);
}

#endif // CF_HAVE_SPI_LINK
