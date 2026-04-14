#pragma once

// RP2040 native USB peripheral = device mode (port 0, to PS3)
#define CFG_TUD_ENABLED         1
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE

// PIO USB = host mode (port 1, to PC via CDC)
#define CFG_TUH_ENABLED         1
#define CFG_TUH_RPI_PIO_USB     1   // use Pico-PIO-USB for the host port

#define CFG_TUSB_OS             OPT_OS_PICO

// ---------------------------------------------------------------------------
// Device-side classes (native USB -> PS3)
// ---------------------------------------------------------------------------
#define CFG_TUD_HID             1
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0
#define CFG_TUD_AUDIO           0

#define CFG_TUD_HID_EP_BUFSIZE  64

// ---------------------------------------------------------------------------
// Host-side classes (PIO USB -> PC)
// We use CDC to receive GIMX packets from the PC.
// Also enable FTDI/CP210x/CH34x for USB-serial adapters plugged into the
// PIO port -- the PC end is just a plain USB cable, no external dongle needed.
// ---------------------------------------------------------------------------
#define CFG_TUH_CDC             1
#define CFG_TUH_CDC_FTDI        1
#define CFG_TUH_CDC_CP210X      1
#define CFG_TUH_CDC_CH34X       1

#define CFG_TUH_DEVICE_MAX      1   // one PC at a time
#define CFG_TUH_HUB             0

// CDC host RX/TX buffer sizes -- must fit at least one GIMX packet (max 66 B)
#define CFG_TUH_CDC_RX_BUFSIZE  256
#define CFG_TUH_CDC_TX_BUFSIZE  64
