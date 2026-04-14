#pragma once

#include <stdint.h>

/*
 * GIMX Adapter Serial Protocol
 * =============================
 * All packets from the PC to the adapter have this structure:
 *
 *   [type: 1 byte] [size: 1 byte] [payload: size bytes]
 *
 * The adapter sends a 1-byte ACK/status back to the PC after each packet.
 *
 * Packet types:
 */

#define BYTE_TYPE        0x00   // Report type byte
#define BYTE_LEN         0x01   // Report length byte
#define BYTE_SEND_REPORT 0x00   // type=0x00 → send HID report immediately

// Report types the PC can send
#define HID_REPORT_TYPE_OUTPUT  0x02  // SET_REPORT output (for force feedback etc.)
#define HID_REPORT_TYPE_FEATURE 0x03  // SET_REPORT feature

// Special type value: PC sends this to trigger a soft reset
#define BYTE_RESET       0xFF

// ACK codes sent back to the PC
#define BYTE_ACK         0x00   // all good
#define BYTE_NAK         0x01   // bad packet / not ready

/*
 * Baud rate used on the UART link between PC and adapter.
 * The GIMX application defaults to 500000 bps.
 */
#define SERIAL_BAUDRATE  500000
