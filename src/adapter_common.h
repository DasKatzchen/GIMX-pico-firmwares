#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Maximum HID report size we will accept over serial.
 * PS3 input report is 48 bytes. Give a little headroom.
 */
#define MAX_HID_REPORT_SIZE 64

/*
 * The current HID report to be sent to the console over USB.
 * Updated by the serial receive path, read by the USB HID path.
 */
extern volatile uint8_t  hid_report[MAX_HID_REPORT_SIZE];
extern volatile uint8_t  hid_report_len;
extern volatile bool     hid_report_pending;  // true = new report waiting to be sent

/*
 * Call once at startup.
 */
void adapter_init(void);

/*
 * Feed one byte from the UART into the adapter state machine.
 * Returns true if a complete, valid packet was received and
 * hid_report/hid_report_pending have been updated.
 */
bool adapter_feed_byte(uint8_t byte);

/*
 * Send a 1-byte response back to the PC over UART.
 * Implemented in main.c (or wherever the UART handle lives).
 */
void adapter_send_ack(uint8_t code);
