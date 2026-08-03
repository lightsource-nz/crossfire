#ifdef CF_HAVE_SPI_LINK

#include <hardware/gpio.h>
#include <hardware/spi.h>

#include "crossfire_internal.h"

// two independent one-way links rather than a single shared bus -- each direction has its
// own master, so there's no arbitration between the two boards to design around. CS
// delimits each 4-byte USB-MIDI Event Packet as its own transaction, so the receiving side
// never needs a custom framing/resync scheme on top of what the hardware already gives for
// free. pin numbers are a starting point, not fixed by the protocol -- adjust freely to
// match actual wiring, just keep OUT_PIN_CS on the same peripheral's canonical pin group as
// its SCK/MOSI (and likewise for IN), since RP2040 SPI pins aren't freely mixable across
// the peripheral's alternate pin groups

// OUT link: this board is master, drives the peer's IN link
#define CF_LINK_OUT_SPI         spi0
#define CF_LINK_OUT_PIN_SCK     18
#define CF_LINK_OUT_PIN_MOSI    19
#define CF_LINK_OUT_PIN_CS      17

// IN link: this board is slave, driven by the peer's OUT link. spi1 has two alternate
// pin groups (GP8-11 and GP12-15) -- this uses the GP12-15 one specifically because
// GP8-11 (SCK=10, MOSI=11, CS=9) exactly matches the PO13 status display's own pins
// (light_display_po13.h). display init is compiled out entirely whenever this link is
// enabled, so that overlap isn't a live runtime conflict today, but there's no reason to
// leave the coincidence in place when a non-overlapping pin group is just as available
#define CF_LINK_IN_SPI          spi1
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

void cf_spi_link_init(void)
{
        spi_init(CF_LINK_OUT_SPI, CF_LINK_BAUDRATE);
        spi_set_format(CF_LINK_OUT_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        gpio_set_function(CF_LINK_OUT_PIN_SCK, GPIO_FUNC_SPI);
        gpio_set_function(CF_LINK_OUT_PIN_MOSI, GPIO_FUNC_SPI);
        // CS is driven manually rather than muxed to the peripheral, so a whole 4-byte
        // burst can be held under one CS assertion via spi_write_blocking() -- same
        // pattern already used for the display driver's bursts
        gpio_init(CF_LINK_OUT_PIN_CS);
        gpio_set_dir(CF_LINK_OUT_PIN_CS, true);
        gpio_put(CF_LINK_OUT_PIN_CS, true);

        // in slave mode the hardware itself uses the CS *input* to know when a
        // transaction starts/ends, so unlike the OUT link, CS here does need to be muxed
        // to the peripheral rather than driven by software
        spi_init(CF_LINK_IN_SPI, CF_LINK_BAUDRATE);
        spi_set_slave(CF_LINK_IN_SPI, true);
        spi_set_format(CF_LINK_IN_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        gpio_set_function(CF_LINK_IN_PIN_SCK, GPIO_FUNC_SPI);
        gpio_set_function(CF_LINK_IN_PIN_MOSI, GPIO_FUNC_SPI);
        gpio_set_function(CF_LINK_IN_PIN_CS, GPIO_FUNC_SPI);

        rx_packet_len = 0;
}

bool cf_spi_link_packet_read(uint8_t packet[4])
{
        while(rx_packet_len < 4 && spi_is_readable(CF_LINK_IN_SPI)) {
                rx_packet_buf[rx_packet_len++] = (uint8_t) spi_get_hw(CF_LINK_IN_SPI)->dr;
        }
        if(rx_packet_len < 4)
                return false;

        for(uint8_t i = 0; i < 4; i++)
                packet[i] = rx_packet_buf[i];
        rx_packet_len = 0;
        return true;
}

void cf_spi_link_packet_write(const uint8_t packet[4])
{
        gpio_put(CF_LINK_OUT_PIN_CS, false);
        spi_write_blocking(CF_LINK_OUT_SPI, packet, 4);
        gpio_put(CF_LINK_OUT_PIN_CS, true);
}

#endif // CF_HAVE_SPI_LINK
