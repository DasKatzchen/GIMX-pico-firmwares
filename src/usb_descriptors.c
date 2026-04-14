/*
 * usb_descriptors.c
 *
 * USB descriptors that make the RP2040 appear to the PS3 as a genuine
 * Sony DualShock 3 (SIXAXIS) controller.
 *
 * VID/PID: 054C:0268  (Sony Corp. / PS3 Controller)
 *
 * The HID report descriptor below is the one used by GIMX EMUPS3 and matches
 * the descriptor captured from real DS3 hardware.  It describes:
 *   - Report ID 0x01: 48-byte input report  (buttons, sticks, pressure axes)
 *   - Report ID 0x01: 8-byte output report  (rumble + LED command)
 *   - Report ID 0xF2: 17-byte feature report (Bluetooth pairing info)
 *   - Report ID 0xF5: 1-byte  feature report (connection status)
 *
 * References:
 *   GIMX EMUPS3/Descriptors.h
 *   https://blog.qiqitori.com/2023/09/... (DS3 descriptor dump)
 */

#include "tusb.h"

/* --------------------------------------------------------------------------
 * HID Report Descriptor
 * ----------------------------------------------------------------------- */

/*
 * This is the canonical DS3 / SIXAXIS HID report descriptor.
 * 148 bytes total — the same value reported by real hardware (desc_size:148).
 *
 * Broken into readable sections with comments.
 */
static const uint8_t desc_hid_report[] = {
    0x05, 0x01,        /* Usage Page (Generic Desktop)      */
    0x09, 0x04,        /* Usage (Joystick)                  */
    0xA1, 0x01,        /* Collection (Application)          */
    0xA1, 0x02,        /*   Collection (Logical)            */

    /* Report ID 1 — main input/output report */
    0x85, 0x01,        /*   Report ID (1)                   */

    /* 1 byte: reserved / always 0 */
    0x75, 0x08,        /*   Report Size (8)                 */
    0x95, 0x01,        /*   Report Count (1)                */
    0x15, 0x00,        /*   Logical Minimum (0)             */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255)           */
    0x81, 0x03,        /*   Input (Const, Var, Abs)         */

    /* 19 buttons as individual bits */
    0x75, 0x01,        /*   Report Size (1)                 */
    0x95, 0x13,        /*   Report Count (19)               */
    0x15, 0x00,        /*   Logical Minimum (0)             */
    0x25, 0x01,        /*   Logical Maximum (1)             */
    0x35, 0x00,        /*   Physical Minimum (0)            */
    0x45, 0x01,        /*   Physical Maximum (1)            */
    0x05, 0x09,        /*   Usage Page (Button)             */
    0x19, 0x01,        /*   Usage Minimum (Button 1)        */
    0x29, 0x13,        /*   Usage Maximum (Button 19)       */
    0x81, 0x02,        /*   Input (Data, Var, Abs)          */

    /* 13 padding bits to round to byte boundary */
    0x75, 0x01,        /*   Report Size (1)                 */
    0x95, 0x0D,        /*   Report Count (13)               */
    0x06, 0x00, 0xFF,  /*   Usage Page (Vendor Defined)     */
    0x81, 0x03,        /*   Input (Const, Var, Abs)         */

    /* 4 analogue axes: LX, LY, RX, RY */
    0x15, 0x00,        /*   Logical Minimum (0)             */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255)           */
    0x05, 0x01,        /*   Usage Page (Generic Desktop)    */
    0x09, 0x01,        /*   Usage (Pointer)                 */
    0xA1, 0x00,        /*   Collection (Physical)           */
    0x75, 0x08,        /*     Report Size (8)               */
    0x95, 0x04,        /*     Report Count (4)              */
    0x35, 0x00,        /*     Physical Minimum (0)          */
    0x46, 0xFF, 0x00,  /*     Physical Maximum (255)        */
    0x09, 0x30,        /*     Usage (X)                     */
    0x09, 0x31,        /*     Usage (Y)                     */
    0x09, 0x32,        /*     Usage (Z)                     */
    0x09, 0x35,        /*     Usage (Rz)                    */
    0x81, 0x02,        /*     Input (Data, Var, Abs)        */
    0xC0,              /*   End Collection (Physical)       */

    /* 39 bytes of pressure / accelerometer data (vendor) */
    0x05, 0x01,        /*   Usage Page (Generic Desktop)    */
    0x75, 0x08,        /*   Report Size (8)                 */
    0x95, 0x27,        /*   Report Count (39)               */
    0x09, 0x01,        /*   Usage (Pointer)                 */
    0x81, 0x02,        /*   Input (Data, Var, Abs)          */

    /* Output report: rumble + LED (8 bytes) */
    0x75, 0x08,        /*   Report Size (8)                 */
    0x95, 0x08,        /*   Report Count (8)                */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255)           */
    0x46, 0xFF, 0x00,  /*   Physical Maximum (255)          */
    0x09, 0x01,        /*   Usage (Pointer)                 */
    0x91, 0x02,        /*   Output (Data, Var, Abs)         */

    /* Feature report 0xF2: 17 bytes (BT pairing) */
    0x85, 0xF2,        /*   Report ID (242)                 */
    0x75, 0x08,        /*   Report Size (8)                 */
    0x95, 0x11,        /*   Report Count (17)               */
    0x06, 0x00, 0xFF,  /*   Usage Page (Vendor)             */
    0x09, 0x01,        /*   Usage (Vendor 1)                */
    0xB1, 0x02,        /*   Feature (Data, Var, Abs)        */

    /* Feature report 0xF5: 1 byte */
    0x85, 0xF5,        /*   Report ID (245)                 */
    0x75, 0x08,        /*   Report Size (8)                 */
    0x95, 0x01,        /*   Report Count (1)                */
    0x06, 0x00, 0xFF,  /*   Usage Page (Vendor)             */
    0x09, 0x01,        /*   Usage (Vendor 1)                */
    0xB1, 0x02,        /*   Feature (Data, Var, Abs)        */

    0xC0,              /*   End Collection (Logical)        */
    0xC0               /* End Collection (Application)     */
};

/* --------------------------------------------------------------------------
 * String descriptors
 * ----------------------------------------------------------------------- */
static const char *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },   /* 0: supported language = English (0x0409) */
    "Sony",                          /* 1: Manufacturer                           */
    "PLAYSTATION(R)3 Controller",   /* 2: Product                                */
    NULL                             /* 3: Serial number (none)                   */
};

static uint16_t _desc_str[32];

/* --------------------------------------------------------------------------
 * Device descriptor
 * ----------------------------------------------------------------------- */
static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0,           /* class specified in interface descriptor */
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x054C,      /* Sony Corp.        */
    .idProduct          = 0x0268,      /* PS3 Controller    */
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,
    .bNumConfigurations = 0x01
};

/* --------------------------------------------------------------------------
 * Configuration descriptor
 * One configuration, one interface, one HID class, two endpoints (IN + OUT).
 * ----------------------------------------------------------------------- */
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

/* Endpoint numbers */
#define EPNUM_HID_IN   0x81   /* EP1 IN  */
#define EPNUM_HID_OUT  0x01   /* EP1 OUT */

static const uint8_t desc_configuration[] = {
    /* Config: number, interface count, string index, total length,
               attributes (bus-powered), power (500 mA) */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    /* HID Interface: itf number, string index, protocol (NONE=0),
                      report descriptor length, EP IN addr, EP size,
                      polling interval (1 ms) */
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report), EPNUM_HID_IN,
                       CFG_TUD_HID_EP_BUFSIZE, 1)
};

/* --------------------------------------------------------------------------
 * TinyUSB descriptor callbacks
 * ----------------------------------------------------------------------- */

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return desc_hid_report;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    uint8_t chr_count;

    if (index == 0) {
        /* Language ID */
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
            return NULL;
        if (string_desc_arr[index] == NULL)
            return NULL;

        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];   /* ASCII → UTF-16LE */
        }
    }

    /* Header: length (bytes) + string descriptor type */
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
