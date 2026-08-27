/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

#if CFG_TUSB_MCU == OPT_MCU_RP2040
// change to 1 if using pico-pio-usb as host controller for raspberry rp2040
#define CFG_TUH_RPI_PIO_USB   0
#define BOARD_TUH_RHPORT      CFG_TUH_RPI_PIO_USB
#endif

// RHPort number used for host can be defined by board.mk, default to port 0
#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT      0
#endif

// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUH_MAX_SPEED
#define BOARD_TUH_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

// Enable Host stack
#define CFG_TUH_ENABLED       1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUH_MAX_SPEED     BOARD_TUH_MAX_SPEED

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// CONFIGURATION
//--------------------------------------------------------------------

// Size of buffer to hold descriptors and other data used for enumeration. 256 was too small
// for a composite audio+MIDI device (its full config descriptor -- Audio Control IF plus
// MIDIStreaming IF plus several jack descriptors -- exceeds it, so usbh.c's
// "total_len <= CFG_TUH_ENUMERATION_BUFSIZE" assert silently bails out of enumeration with no
// mount callback and no visible error, since CFG_TUSB_DEBUG is 0)
#define CFG_TUH_ENUMERATION_BUFSIZE 512

//   HUBS IN THE WHOLE TREE, not "how many hubs you may plug in" -- and that distinction cost a
// full debugging session, so it is worth stating plainly. This was 1, and hub mode
// (CROSSFIRE_ENABLE_USB_HUB) then failed on the first real hub it met: the hub enumerated fine
// at address 5, and the device on ITS port 1 turned out to be a second hub (bDeviceClass 0x09).
// Physically one plastic box; electrically two chips in series, which is how most hubs with more
// than four ports are built.
//   the failure is silent and looks nothing like a configuration limit. enum_get_new_address()
// allocates hub addresses from a window exactly CFG_TUH_HUB wide, returns 0 when it is full, and
// the caller's TU_ASSERT(new_addr != 0,) aborts enumeration -- which the hub driver then retries,
// forever, at full speed. Nothing mounts and nothing says why. TinyUSB does log "All addresses
// are occupied, try to increase CFG_TUH_HUB value", but only at TU_LOG1, and CFG_TUSB_DEBUG is 0
// here. The symptom on the bench is a board that looks alive, spins in tuh_task(), and never
// calls a mount callback.
//   2 covers a single chained hub, which is what the bench rig is and what a 7-port hub
// generally is. A deeper tree (a hub behind a dock behind a hub) needs more; the cost is one
// usbh_device_t plus a hub interface slot each.
#define CFG_TUH_HUB                 2

//   max device support, EXCLUDING the hub itself -- TinyUSB sizes its device table as
// TOTAL_DEVICES = CFG_TUH_DEVICE_MAX + CFG_TUH_HUB (usbh.c), so 4 here means four instruments
// on the hub's downstream ports plus the hub, not three plus the hub.
//   this number is the same four as crossfire_internal.h's CF_MAX_DEVICES_USB, and they must
// stay equal: TinyUSB never hands out a mount index at or beyond CFG_TUH_MIDI, which is what
// lets the forwarding engine's device table be indexed by it directly.
#define CFG_TUH_DEVICE_MAX          (CFG_TUH_HUB ? 4 : 1)

// enable the USB-MIDI host class driver (disabled by default upstream), and track up to
// one mounted MIDI interface per possible device
#define CFG_TUH_MIDI                CFG_TUH_DEVICE_MAX

// Max endpoint per device
#define CFG_TUH_ENDPOINT_MAX        8

// Enable tuh_edpt_xfer() API
#define CFG_TUH_API_EDPT_XFER       1

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
