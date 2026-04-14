#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_HID_REPORT_SIZE 64

extern volatile uint8_t  hid_report[MAX_HID_REPORT_SIZE];
extern volatile uint8_t  hid_report_len;
extern volatile bool     hid_report_pending;

void adapter_init(void);
bool adapter_feed_byte(uint8_t byte);

/*
 * Implemented in main.c — sends one ACK byte back to the PC.
 * In the PIO-USB version this pushes to tx_ring (core1 drains it).
 */
void adapter_send_ack(uint8_t code);
