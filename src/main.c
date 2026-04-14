/*
 * main.c — GIMX EMUPS3 port for RP2040
 *
 * Hardware connections (Raspberry Pi Pico pinout):
 *
 *   GP0  (UART0 TX) → RX pin of USB-to-UART adapter (e.g. CP2102) → PC
 *   GP1  (UART0 RX) → TX pin of USB-to-UART adapter               ← PC
 *   GND             → GND of USB-to-UART adapter
 *   USB             → PS3 console (presents as DualShock 3)
 *
 * The PC runs the GIMX application, which sends HID reports over the UART
 * at 500 kbps.  This firmware receives those reports and forwards them to
 * the PS3 via USB HID.
 *
 * IMPORTANT: Both the USB port and the UART adapter must be powered.
 *   - Power the Pico via its USB port (connected to the PS3).
 *   - The USB-to-UART adapter takes power from the PC USB port.
 *   - Do NOT connect the VBUS/5V pins between the Pico and the adapter
 *     (they have their own power).  Only connect TX, RX, GND.
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "adapter_common.h"
#include "adapter_protocol.h"

/* --------------------------------------------------------------------------
 * Pin / UART configuration
 * ----------------------------------------------------------------------- */
#define GIMX_UART        uart0
#define GIMX_UART_TX_PIN 0
#define GIMX_UART_RX_PIN 1
#define GIMX_UART_BAUD   500000

/* Optional: onboard LED blinks when a report is forwarded */
#define LED_PIN          PICO_DEFAULT_LED_PIN

/* --------------------------------------------------------------------------
 * Forward declarations
 * ----------------------------------------------------------------------- */
static void uart_init_gimx(void);
static void hid_task(void);

/* --------------------------------------------------------------------------
 * adapter_send_ack() — called from adapter_common.c
 * Sends a single status byte back to the PC.
 * ----------------------------------------------------------------------- */
void adapter_send_ack(uint8_t code)
{
    uart_putc_raw(GIMX_UART, (char)code);
}

/* --------------------------------------------------------------------------
 * main()
 * ----------------------------------------------------------------------- */
int main(void)
{
    /* Run the RP2040 at 120 MHz — gives USB some headroom */
    set_sys_clock_khz(120000, true);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    uart_init_gimx();
    adapter_init();
    tusb_init();

    while (true) {
        tud_task();   /* TinyUSB device task — must be called frequently */
        hid_task();   /* Drain UART, update + send HID report            */
    }

    return 0;
}

/* --------------------------------------------------------------------------
 * UART initialisation
 * ----------------------------------------------------------------------- */
static void uart_init_gimx(void)
{
    uart_init(GIMX_UART, GIMX_UART_BAUD);
    uart_set_format(GIMX_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(GIMX_UART, true);

    gpio_set_function(GIMX_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(GIMX_UART_RX_PIN, GPIO_FUNC_UART);
}

/* --------------------------------------------------------------------------
 * hid_task() — called every iteration of the main loop
 *
 * Drains all available UART bytes into the adapter state machine.
 * When a complete report is ready AND TinyUSB has an idle IN endpoint,
 * the report is sent to the PS3.
 * ----------------------------------------------------------------------- */
static void hid_task(void)
{
    /* Drain every available byte from the UART FIFO */
    while (uart_is_readable(GIMX_UART)) {
        uint8_t byte = (uint8_t)uart_getc(GIMX_UART);
        adapter_feed_byte(byte);
    }

    /*
     * If a new report arrived and TinyUSB is ready to send, push it.
     *
     * tud_hid_ready() checks that:
     *   1. The device is enumerated (connected and configured by the PS3).
     *   2. The previous IN transfer has completed.
     *
     * The PS3 polls at 1 ms intervals, so we will rarely wait more than 1 ms.
     */
    if (hid_report_pending && tud_hid_ready()) {
        uint8_t report_copy[MAX_HID_REPORT_SIZE];
        uint8_t report_len;

        /* Snapshot atomically (single-core, no preemption) */
        report_len = hid_report_len;
        memcpy(report_copy, (const void *)hid_report, report_len);
        hid_report_pending = false;

        /*
         * tud_hid_report(report_id, data, len)
         *
         * report_id=0 means the report ID byte is included in `data`
         * (which it is — hid_report[0] == 0x01).
         * Pass 0 here so TinyUSB sends the buffer verbatim.
         */
        tud_hid_report(0, report_copy, report_len);

        gpio_put(LED_PIN, !gpio_get(LED_PIN));  /* toggle LED on each report */
    }
}

/* --------------------------------------------------------------------------
 * TinyUSB HID callbacks (required by the stack)
 * ----------------------------------------------------------------------- */

/*
 * GET_REPORT request from host (PS3).
 * The PS3 issues this during enumeration to read the initial controller state.
 * Return the current report buffer.
 */
uint16_t tud_hid_get_report_cb(uint8_t itf,
                                uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer,
                                uint16_t reqlen)
{
    (void)itf;
    (void)report_id;
    (void)report_type;

    uint8_t len = (hid_report_len < reqlen) ? hid_report_len : (uint8_t)reqlen;
    memcpy(buffer, (const void *)hid_report, len);
    return len;
}

/*
 * SET_REPORT request from host (PS3 → adapter).
 *
 * The PS3 sends output reports (rumble + LED) and feature reports
 * (Bluetooth pairing) via SET_REPORT.
 *
 * For a complete implementation, forward these back to the PC over UART
 * so GIMX can react (e.g. for force feedback passthrough).
 *
 * Minimal implementation: just absorb them silently for now.
 * To enable FFB forwarding, uncomment the uart_write_blocking() call.
 */
void tud_hid_set_report_cb(uint8_t itf,
                            uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer,
                            uint16_t bufsize)
{
    (void)itf;
    (void)report_type;

    /*
     * Optional: echo the SET_REPORT payload back to the PC.
     * Format: [type=0x01] [report_id] [len] [payload]
     * Uncomment if GIMX force-feedback / LED feedback is needed.
     *
     * uint8_t header[3] = { 0x01, report_id, (uint8_t)bufsize };
     * uart_write_blocking(GIMX_UART, header, 3);
     * uart_write_blocking(GIMX_UART, buffer, bufsize);
     */

    (void)report_id;
    (void)buffer;
    (void)bufsize;
}
