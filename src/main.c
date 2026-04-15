/*
 * main.c -- GIMX EMUPS3 port for RP2040 (PIO-USB serial version)
 *
 * Architecture
 * ============
 *
 *   Core 0  ──  tud_task()  ──  native USB port  ──►  PS3 (HID device)
 *   Core 1  ──  tuh_task()  ──  PIO USB port     ──►  PC  (CDC host)
 *
 * Data flow:
 *   PC ──USB-CDC──► PIO port (core1) ──rx_ring──► adapter parser (core0)
 *                                                       │
 *                                           HID report buffer
 *                                                       │
 *   PS3 ◄──USB-HID────────────────── tud_hid_report() (core0)
 *
 *   ACK bytes: core0 ──tx_ring──► core1 ──CDC write──► PC
 *
 * Hardware wiring
 * ===============
 *
 *   PIO USB port (D+ = GP2, D- = GP3 by default):
 *     Connect a USB cable:  D+ → GP2 via 22Ω,  D- → GP3 via 22Ω
 *     The PC enumerates this as a CDC serial port.
 *     No external USB-UART adapter needed.
 *
 *   Native USB (micro-USB / USB-C on the Pico):
 *     Plug into the PS3 console.
 *
 *   Power:  The Pico is powered by the PS3 USB.
 *           The PC also supplies 5 V through the PIO USB cable — connect a
 *           Schottky diode in series on VBUS from the PIO cable to avoid
 *           back-powering conflicts, OR just don't connect VBUS from the
 *           PIO side (data only) if the Pico is already powered by PS3.
 *
 * PIO USB pin assignment
 * ======================
 *   PIO_USB_DP_PIN  GP2   (D+)    <-- change below if using different pins
 *   D-              GP3   (auto = DP+1, enforced by Pico-PIO-USB)
 *
 * Note: Pico-PIO-USB requires D- = D+ + 1.  Choose any adjacent pair
 *       except GP0/GP1 (used for debug UART).
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "tusb.h"
#include "pio_usb.h"

#include "adapter_common.h"
#include "adapter_protocol.h"
#include "ring_buffer.h"

/* --------------------------------------------------------------------------
 * Configuration
 * ---------------------------------------------------------------------- */
#define LED_PIN          PICO_DEFAULT_LED_PIN

/*
 * PIO USB D+ pin.  D- is implicitly DP+1 (GP3 here).
 * Pico-PIO-USB uses PIO0 and claims 3 state machines + 32 instructions.
 * Change to any free adjacent GPIO pair (must not be GP0/1 if UART debug
 * is enabled, and must not conflict with other peripherals).
 */
#define PIO_USB_DP_PIN   2

/* CDC interface index on the host side (only one CDC device expected) */
#define CDC_ITF          0

/* --------------------------------------------------------------------------
 * adapter_send_ack()  (called from adapter_common.c / core0)
 * Pushes one byte into the tx_ring; core1 drains it to the CDC write.
 * ---------------------------------------------------------------------- */
void adapter_send_ack(uint8_t code)
{
    /* Best-effort: if ring is full the ACK is dropped.
     * The GIMX application on the PC retries on timeout. */
    ring_push(&tx_ring, code);
}

/* ==========================================================================
 * CORE 1 -- PIO USB host task
 * ======================================================================= */

/*
 * CDC mount callback — fires on core1 when the PC's CDC device is detected.
 * Set 500 kbps line coding to match GIMX.
 */
void tuh_cdc_mount_cb(uint8_t idx)
{
    cdc_line_coding_t coding = {
        .bit_rate  = SERIAL_BAUDRATE,   /* 500000 */
        .stop_bits = 0,                  /* 1 stop bit */
        .parity    = 0,                  /* none */
        .data_bits = 8
    };
    tuh_cdc_set_line_coding(idx, &coding, NULL, 0);
    printf("CDC[%u] mounted, set 500kbps\n", idx);
}

void tuh_cdc_umount_cb(uint8_t idx)
{
    printf("CDC[%u] unmounted\n", idx);
}

/*
 * CDC RX callback — fires on core1 when data arrives from the PC.
 * Push every byte into rx_ring for core0 to consume.
 */
void tuh_cdc_rx_cb(uint8_t idx)
{
    uint8_t buf[64];
    uint32_t count = tuh_cdc_read(idx, buf, sizeof(buf));
    for (uint32_t i = 0; i < count; i++) {
        if (!ring_push(&rx_ring, buf[i])) {
            /* Ring full — data lost.  Shouldn't happen at 500 kbps with
             * a 512-byte ring and a cooperative core0 loop. */
            printf("rx_ring FULL, byte dropped!\n");
        }
    }
}

/*
 * core1_main() — runs the PIO USB host stack.
 * Initialise AFTER core0 has set the system clock.
 */
static void core1_main(void)
{
    /* Configure and start PIO USB host on port 1 */
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = PIO_USB_DP_PIN;
    tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
    tuh_init(1);

    printf("core1: PIO USB host started (D+=%d D-=%d)\n",
           PIO_USB_DP_PIN, PIO_USB_DP_PIN + 1);

    while (true) {
        tuh_task();   /* drives PIO USB host enumeration + transfers */

        /* Drain tx_ring and send ACK bytes back to the PC */
        uint8_t ack;
        while (ring_pop(&tx_ring, &ack)) {
            /* tuh_cdc_write is non-blocking; flush separately */
            tuh_cdc_write(CDC_ITF, &ack, 1);
            tuh_cdc_write_flush(CDC_ITF);
        }
    }
}

/* ==========================================================================
 * CORE 0 -- Native USB device task + adapter logic
 * ======================================================================= */

static void hid_task(void)
{
    /* Drain all bytes from rx_ring into the adapter parser */
    uint8_t byte;
    while (ring_pop(&rx_ring, &byte)) {
        adapter_feed_byte(byte);
    }

    /* Forward pending HID report to the PS3 */
    if (hid_report_pending && tud_hid_ready()) {
        uint8_t buf[MAX_HID_REPORT_SIZE];
        uint8_t len = hid_report_len;
        memcpy(buf, (const void *)hid_report, len);
        hid_report_pending = false;

        tud_hid_report(0, buf, len);
        gpio_put(LED_PIN, !gpio_get(LED_PIN));
    }
}

int main(void)
{
    /*
     * System clock MUST be a multiple of 12 MHz for PIO USB bit-banging.
     * 120 MHz is the standard choice (also what the official example uses).
     */
    set_sys_clock_khz(120000, true);

    /* Short delay so PLL is stable before USB starts */
    sleep_ms(10);

    stdio_init_all();   /* debug UART on GP0/GP1 at 115200 (optional) */

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    adapter_init();

    /*
     * Launch core1 BEFORE initialising the native USB device.
     * core1 sets up the PIO USB host; core0 sets up the USB device.
     * Both call their respective tXX_init() independently.
     */
    multicore_reset_core1();
    multicore_launch_core1(core1_main);

    /* Init native USB device stack (roothub port 0) */
    tud_init(0);

    printf("core0: native USB device started\n");

    while (true) {
        tud_task();    /* TinyUSB device task */
        hid_task();    /* parse RX ring, send HID reports */
    }

    return 0;
}

/* ==========================================================================
 * TinyUSB HID device callbacks  (called from core0 via tud_task)
 * ======================================================================= */

uint16_t tud_hid_get_report_cb(uint8_t itf,
                                uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer,
                                uint16_t reqlen)
{
    (void)itf; (void)report_id; (void)report_type;
    uint8_t len = (hid_report_len < (uint8_t)reqlen) ? hid_report_len : (uint8_t)reqlen;
    memcpy(buffer, (const void *)hid_report, len);
    return len;
}

void tud_hid_set_report_cb(uint8_t itf,
                            uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer,
                            uint16_t bufsize)
{
    (void)itf; (void)report_type;
    /*
     * PS3 output / feature reports (rumble, LEDs, BT pairing).
     * Forward them back to the PC over CDC so GIMX can act on them.
     *
     * Format sent to PC: [0x01][report_id][len][payload...]
     * Uncomment to enable force-feedback passthrough.
     */
    /*
    uint8_t hdr[3] = { 0x01, report_id, (uint8_t)bufsize };
    for (int i = 0; i < 3;         i++) ring_push(&tx_ring, hdr[i]);
    for (int i = 0; i < bufsize;   i++) ring_push(&tx_ring, buffer[i]);
    */
    (void)report_id; (void)buffer; (void)bufsize;
}
