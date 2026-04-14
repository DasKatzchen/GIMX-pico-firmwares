#pragma once

// RP2040 native USB in device mode
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE
#define CFG_TUSB_OS             OPT_OS_PICO

// Only HID device class needed
#define CFG_TUD_HID             1
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0
#define CFG_TUD_AUDIO           0

// HID endpoint buffer size - PS3 uses 64-byte reports
#define CFG_TUD_HID_EP_BUFSIZE  64

// Stack size (optional but explicit is good)
#define CFG_TUD_TASK_QUEUE_SZ   16
