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

//   PORT IDS MEAN DIFFERENT THINGS PER PLATFORM, which is easy to miss when reading one branch.
// On RP2 the id is light_ioport's PORT_SPI_n constant; on STM32 it is the SPI instance number
// itself, so 2 is SPI2 -- screen-test's H7 display passes 4 for SPI4 the same way.
#if(LIGHT_SYSTEM == SYSTEM_CMSIS)

//   STM32H743 on the WeAct MiniSTM32H7xx. Both pin groups are checked against what the board
// actually commits, taken from WeAct's own sources by way of Zephyr's board DTS rather than
// guessed:
//     QSPI flash   PB2, PB6, PD11, PD12, PD13, PE2
//     SPI flash    PB3 (SCK), PB4 (MISO), PD7 (MOSI), PD6 (CS)
//     SD card      PC8-PC12, PD2, PD4
//     display      PE11-PE14 (ST7735, screen-test only)
//     camera I2C   PB8, PB9
//     console      PA9, PA10      USB  PA11, PA12      SWD  PA13, PA14
//     LED PE3      KEY PC13
//
// IN link: this board is slave, driven by the peer's OUT link. SPI2 on PB12/13/15, which the
// DTS shows unassigned -- no on-board peripheral touches them.
#define CF_LINK_IN_PORT         2
#define CF_LINK_IN_PIN_SCK      LIGHT_IOPORT_PIN_STM32('B', 13)
#define CF_LINK_IN_PIN_MOSI     LIGHT_IOPORT_PIN_STM32('B', 15)
#define CF_LINK_IN_PIN_CS       LIGHT_IOPORT_PIN_STM32('B', 12)

//   OUT link: SPI4 on the DISPLAY's pins, which are free precisely because a crossfire build on
// this board is headless -- the panel is a Pico expansion board that does not exist here. If a
// display is ever added to an H7 crossfire, these two collide and this is the comment that says
// so.
//   NOT SPI3 on PB3-5, which was the first choice and was wrong: PB3 and PB4 are the on-board
// SPI flash's clock and MISO. A master drives SCK unconditionally -- no chip select gates that --
// so this side would have clocked the flash on every link write. PB3 is also TRACESWO.
#define CF_LINK_OUT_PORT        4
#define CF_LINK_OUT_PIN_SCK     LIGHT_IOPORT_PIN_STM32('E', 12)
#define CF_LINK_OUT_PIN_MOSI    LIGHT_IOPORT_PIN_STM32('E', 14)
#define CF_LINK_OUT_PIN_CS      LIGHT_IOPORT_PIN_STM32('E', 11)

#else

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
//   THE DATA PIN HERE IS GP12, NOT GP15, AND THAT IS NOT A TYPO. The RP2 SPI block names its
// data pins RX and TX rather than MOSI/MISO, and which one carries the incoming data depends on
// the role, not on the name: a master transmits on TX (GP19 on the OUT link above), while a
// slave RECEIVES on RX. GP15 is spi1 TX, so naming it here muxed it as an OUTPUT that this
// board drove against the peer's MOSI -- two drivers on one wire -- while the slave listened on
// GP12, which nothing was connected to, and dutifully clocked in zeros. Bytes arrived with
// perfect framing and every one of them was 0x00.
//   the STM32 branch above does not have this trap because an STM32 SPI has dedicated MOSI and
// MISO pins, so its slave input really is the pin called MOSI.
#define CF_LINK_IN_PORT         PORT_SPI_1
#define CF_LINK_IN_PIN_SCK      14
#define CF_LINK_IN_PIN_MOSI     12
#define CF_LINK_IN_PIN_CS       13

#endif  // LIGHT_SYSTEM == SYSTEM_CMSIS

//   8MHz, raised from 1MHz. The sends are BLOCKING and a packet is only 4 bytes, so the rate
// translates directly into CPU time held by each forwarded packet: 1MHz meant ~41us of spinning
// per packet on both boards, and 4 bytes is far too small for DMA to pay back its setup, so the
// clock is the only lever there is.
//   THE CEILING IS THE RECEIVING END, NOT THE WIRE. An RP2 slave is a PL022, which requires its
// peripheral clock to be at least 12x SCK -- clk_peri is 150MHz on the Pico 2 (read off the
// board, not assumed), putting the hard limit at 12.5MHz. 8MHz sits comfortably under that and
// leaves room for a slower clk_peri on an RP2040 peer.
//   the achieved rates differ per platform because both dividers are powers of two from
// different kernel clocks: the RP2 master lands on 8MHz exactly, the H743's SPI4 on 6.25MHz
// (100MHz APB2 / 16). Both are what light_ioport_set_spi_clock() reports back.
//   if this ever proves marginal on breadboard jumpers, lowering it is the first thing to try --
// the link has no error detection, so marginal wiring shows up as corrupt MIDI, not as an error.
#define CF_LINK_BAUDRATE        (8 * 1000 * 1000)

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
        //   MODE 1, AND BOTH ENDS MUST AGREE. A packet is one 4-byte burst under a single CS
        // assertion, and an RP2 SPI slave will only accept back-to-back frames like that with
        // CPHA=1 -- in mode 0 it takes the CS edge as the frame start and delivers the first
        // byte followed by zeros. Set on every context, master and slave, so the two boards
        // stay in step regardless of which side a given build is.
        light_ioport_set_spi_mode(link_out, LIGHT_IOPORT_SPI_MODE_1);

        // in slave mode the hardware itself uses the CS *input* to know when a
        // transaction starts/ends, so unlike the OUT link, CS here does need to be muxed
        // to the peripheral rather than driven by software
        //   no set_spi_clock() on this one: a slave is clocked by the far end and has no baud
        // rate of its own. The link's speed is set by the OUT master above, on both boards.
        link_in = light_ioport_setup_io_spi_slave(CF_LINK_IN_PORT,
                                        CF_LINK_IN_PIN_CS, CF_LINK_IN_PIN_SCK, CF_LINK_IN_PIN_MOSI);
        //   the slave needs the phase too. It has no clock of its own, which makes it tempting
        // to assume the master imposes everything -- but CPHA decides which edge this end
        // SAMPLES on, and no master can set that remotely.
        light_ioport_set_spi_mode(link_in, LIGHT_IOPORT_SPI_MODE_1);

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
