/*
 * adapter_common.c
 *
 * Receives GIMX protocol packets over UART and updates the HID report buffer.
 *
 * Original AVR/LUFA implementation: matlo/GIMX-firmwares
 * RP2040/TinyUSB port: this file
 *
 * Protocol recap:
 *   PC → Adapter:  [type 1B] [len 1B] [payload len bytes]
 *   Adapter → PC:  [ack 1B]
 *
 * type == 0x00 ("SEND_REPORT") is the only type used in normal operation.
 * The payload is the raw HID input report to forward to the console via USB.
 */

#include "adapter_common.h"
#include "adapter_protocol.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Shared state (read by USB HID callbacks, written here)
 * ---------------------------------------------------------------------- */
volatile uint8_t  hid_report[MAX_HID_REPORT_SIZE];
volatile uint8_t  hid_report_len   = 0;
volatile bool     hid_report_pending = false;

/* -------------------------------------------------------------------------
 * Internal parser state machine
 * ---------------------------------------------------------------------- */
typedef enum {
    STATE_WAIT_TYPE,   // waiting for packet type byte
    STATE_WAIT_LEN,    // waiting for payload length byte
    STATE_READ_PAYLOAD // reading payload bytes
} parser_state_t;

static parser_state_t  state        = STATE_WAIT_TYPE;
static uint8_t         pkt_type     = 0;
static uint8_t         pkt_len      = 0;
static uint8_t         pkt_buf[MAX_HID_REPORT_SIZE];
static uint8_t         pkt_idx      = 0;

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

void adapter_init(void)
{
    state             = STATE_WAIT_TYPE;
    pkt_idx           = 0;
    hid_report_len    = 0;
    hid_report_pending = false;
    memset(pkt_buf, 0, sizeof(pkt_buf));

    /* Zero the report so we start with a neutral controller state */
    memset((void *)hid_report, 0, sizeof(hid_report));
    /*
     * PS3 analogue sticks are centred at 0x80.
     * Bytes 6-9 in the PS3 input report are LX, LY, RX, RY.
     * Pre-set them so the first USB poll returns a valid idle state.
     */
    hid_report[0]  = 0x01; /* report ID */
    hid_report[6]  = 0x80; /* LX centre */
    hid_report[7]  = 0x80; /* LY centre */
    hid_report[8]  = 0x80; /* RX centre */
    hid_report[9]  = 0x80; /* RY centre */
    hid_report_len = 48;
}

bool adapter_feed_byte(uint8_t byte)
{
    bool packet_complete = false;

    switch (state) {

        case STATE_WAIT_TYPE:
            pkt_type = byte;
            state    = STATE_WAIT_LEN;
            break;

        case STATE_WAIT_LEN:
            pkt_len = byte;

            if (pkt_len == 0) {
                /*
                 * Zero-length packet: treat as a keep-alive / ping.
                 * ACK immediately and stay ready.
                 */
                adapter_send_ack(BYTE_ACK);
                state = STATE_WAIT_TYPE;
            } else if (pkt_len > MAX_HID_REPORT_SIZE) {
                /* Payload too large — reject and re-sync */
                adapter_send_ack(BYTE_NAK);
                state = STATE_WAIT_TYPE;
            } else {
                pkt_idx = 0;
                state   = STATE_READ_PAYLOAD;
            }
            break;

        case STATE_READ_PAYLOAD:
            pkt_buf[pkt_idx++] = byte;

            if (pkt_idx == pkt_len) {
                /* Full packet received */
                if (pkt_type == BYTE_SEND_REPORT) {
                    /*
                     * Copy payload into the shared HID report buffer.
                     * Critical section: USB interrupt may be reading it.
                     * On RP2040 a simple flag + memcpy is safe here because
                     * TinyUSB runs on the same core as us (no preemption).
                     * If you move tud_task() to core1, use a mutex instead.
                     */
                    memcpy((void *)hid_report, pkt_buf, pkt_len);
                    hid_report_len     = pkt_len;
                    hid_report_pending = true;
                    packet_complete    = true;
                }
                /* ACK every valid packet regardless of type */
                adapter_send_ack(BYTE_ACK);
                state = STATE_WAIT_TYPE;
            }
            break;
    }

    return packet_complete;
}
