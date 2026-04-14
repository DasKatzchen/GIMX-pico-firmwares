# GIMX EMUPS3 — RP2040 Port (PIO USB serial)

Ports [GIMX EMUPS3](https://github.com/matlo/GIMX-firmwares) to RP2040 with
**no external USB-UART adapter** — the PC connects via a plain USB cable to
a second USB port implemented with Pico-PIO-USB.

```
PC ──USB cable──► PIO USB port (GP2/GP3)  ←─ GIMX serial (CDC 500 kbps)
                         │ core1 → rx_ring → core0
PS3 ◄──USB cable──── Native USB port      ─── HID device (DualShock 3)
```

---

## Hardware wiring

```
Raspberry Pi Pico
─────────────────
  GP2  ──[22Ω]──  D+  ┐
  GP3  ──[22Ω]──  D-  ├── USB-A or USB-C breakout → PC
  GND             GND ┘
  
  USB (native micro-USB/USB-C) ──────────────────────► PS3
```

**Series 22 Ω resistors on D+/D− are strongly recommended** by Pico-PIO-USB
for signal integrity (reduces reflections at full-speed 12 Mbps).

> **Power note:** If the Pico is powered by the PS3 USB, add a Schottky diode
> (e.g. 1N5819) in series on the VBUS line coming from the PC's PIO cable to
> prevent back-powering conflicts.  Or simply leave VBUS unconnected from the
> PIO cable and run power-only from the PS3.

---

## Dependencies

| Dependency | Location |
|---|---|
| [pico-sdk](https://github.com/raspberrypi/pico-sdk) ≥ 1.5 | `$PICO_SDK_PATH` |
| [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB) | `../Pico-PIO-USB` (or set `PICO_PIO_USB_PATH`) |

```bash
# Clone both repos side-by-side
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git submodule update --init && cd ..

git clone https://github.com/sekigon-gonnoc/Pico-PIO-USB.git

git clone <this-repo> emups3-rp2040
```

---

## Build

```bash
export PICO_SDK_PATH=/path/to/pico-sdk

cd emups3-rp2040
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Output: `build/emups3.uf2`

Hold **BOOTSEL** while plugging in → drag `emups3.uf2` onto the mass storage
device that appears.

---

## Usage

1. Flash the firmware.
2. Connect the **native USB** port to the PS3.
3. Connect a USB cable from the **PIO USB port** (GP2/GP3) to the PC.
4. On the PC, open GIMX and select the serial port corresponding to the Pico's
   PIO CDC device (it will enumerate as a generic CDC serial port).
5. The PS3 should see a DualShock 3 controller.
6. The onboard LED toggles on every forwarded HID report.

---

## Project structure

```
emups3-rp2040/
├── CMakeLists.txt
└── src/
    ├── tusb_config.h       ← TinyUSB: device (HID) + host (CDC via PIO)
    ├── adapter_protocol.h  ← GIMX wire protocol constants
    ├── adapter_common.h/c  ← Serial packet parser (unchanged from AVR port)
    ├── ring_buffer.h/c     ← Lock-free SPSC ring buffer (inter-core comms)
    ├── usb_descriptors.c   ← DS3 VID/PID 054C:0268 + HID descriptor
    └── main.c              ← Dual-core setup, TinyUSB callbacks
```

---

## Architecture

```
core1 (PIO USB host)          core0 (native USB device)
────────────────────          ─────────────────────────
tuh_task()                    tud_task()
  │                             │
tuh_cdc_rx_cb()               hid_task()
  │  push bytes                 │  pop bytes
  └──────► rx_ring ────────────►│
                                 adapter_feed_byte()
                                 │
            tx_ring ◄────────────┘  adapter_send_ack()
              │
tuh_cdc_write() ◄──── core1 drains tx_ring
```

| Role | Core | USB port | TinyUSB stack |
|---|---|---|---|
| PS3 controller (HID device) | Core 0 | Native USB (GP-USB) | `tud_task()` |
| PC serial link (CDC host)   | Core 1 | PIO USB (GP2/GP3)   | `tuh_task()` |

The two rings (`rx_ring`, `tx_ring`) are lock-free SPSC structures — safe
between the two cores without mutexes because each ring has exactly one writer
and one reader.

---

## Porting notes vs the UART version

| UART version | PIO USB version |
|---|---|
| CP2102/CH340 dongle required | Plain USB cable to PC |
| `hardware_uart` + `uart_getc()` | `tuh_cdc_rx_cb()` + `rx_ring` |
| `uart_putc()` for ACK | `tx_ring` → `tuh_cdc_write()` |
| Single-core | Dual-core (host on core1, device on core0) |
| `uart_init(uart0, 500000)` | `tuh_cdc_set_line_coding()` @ 500 kbps |

The adapter parser (`adapter_common.c`) and GIMX protocol (`adapter_protocol.h`)
are identical between both versions.

---

## Force feedback / rumble passthrough

Uncomment the `ring_push` block in `tud_hid_set_report_cb()` in `main.c`
to forward PS3 output/feature reports back to the PC over CDC.

---

## Changing the PIO USB pins

Edit `PIO_USB_DP_PIN` in `main.c`.  D− is always D+ + 1 (Pico-PIO-USB
constraint).  Avoid GP0/GP1 (debug UART) and any pin pairs already used.

---

## License

Derived from [GIMX-firmwares](https://github.com/matlo/GIMX-firmwares) by
Mathieu Laurendeau.  Original code GPL-3.0.  This port inherits that license.
Pico-PIO-USB is MIT licensed.
