# GIMX EMUPS3 — RP2040 Port

Ports the [GIMX EMUPS3 firmware](https://github.com/matlo/GIMX-firmwares)
from AVR/LUFA to RP2040/TinyUSB.

The Pico presents itself as a Sony DualShock 3 (VID `054C`, PID `0268`) to the
PS3 over its native USB port, and receives HID reports from the PC running GIMX
over a hardware UART at 500 kbps.

This is partially intended as an exercise in using AI to assist in coding,
In order to see whether they're more help or hindrance.

---

## Hardware wiring

```
  PC (USB)
     │
  CP2102 / CH340 / any 3.3 V USB-UART adapter
     │  TX ──→ GP1  (UART0 RX)
     │  RX ←── GP0  (UART0 TX)
     │  GND ── GND
     │
  Raspberry Pi Pico
     │
  USB (native) ──→ PS3 console
```

> **Power note:** the Pico is bus-powered by the PS3 via USB.
> The UART adapter is bus-powered by the PC.
> Connect **only TX, RX, GND** between them — do **not** bridge their 5 V / VBUS pins.

---

## Prerequisites

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) ≥ 1.5 (TinyUSB bundled)
- `arm-none-eabi-gcc`, `cmake`, `ninja` (or make)
- `PICO_SDK_PATH` environment variable set

### Install Pico SDK (if not already installed)

```bash
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init      # pulls TinyUSB and other dependencies
export PICO_SDK_PATH=$(pwd)
```

---

## Build

```bash
git clone <this-repo>
cd emups3-rp2040
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Output: `build/emups3.uf2`

### Flash

Hold **BOOTSEL** on the Pico while plugging in USB → it appears as a mass
storage device.  Copy `emups3.uf2` onto it.  The Pico reboots automatically.

---

## Usage

1. Flash the firmware (above).
2. Connect the Pico's USB port to the PS3.
3. Connect the UART adapter between the Pico (GP0/GP1/GND) and the PC.
4. On the PC, start the GIMX application and select the serial port of the UART adapter.
5. The PS3 should enumerate the Pico as a DualShock 3.
6. The onboard LED toggles on every forwarded report.

---

## Project structure

```
emups3-rp2040/
├── CMakeLists.txt
└── src/
    ├── tusb_config.h       # TinyUSB configuration
    ├── adapter_protocol.h  # GIMX serial protocol constants
    ├── adapter_common.h    # Shared state / API
    ├── adapter_common.c    # Serial packet parser (ported from GIMX AVR)
    ├── usb_descriptors.c   # DS3 USB + HID descriptors (TinyUSB callbacks)
    └── main.c              # Main loop, UART init, HID callbacks
```

---

## Porting notes

| AVR / LUFA | RP2040 / TinyUSB |
|---|---|
| `USB_Init()` + `USB_USBTask()` | `tusb_init()` + `tud_task()` |
| `HID_Device_USBTask()` | `tud_task()` |
| `CALLBACK_HID_Device_CreateHIDReport()` | `tud_hid_get_report_cb()` |
| `CALLBACK_HID_Device_ProcessHIDReport()` | `tud_hid_set_report_cb()` |
| `HID_Device_SendHIDReport()` | `tud_hid_report()` |
| AVR USART at 500 kbps | `uart0` on GP0/GP1 via `hardware_uart` |
| LUFA CDC virtual serial | Not needed — hardware UART used directly |
| `avr-gcc` + Makefile | `arm-none-eabi-gcc` + CMake |

### Force feedback / rumble

The PS3 sends rumble commands as HID output reports (SET_REPORT type 0x02).
`tud_hid_set_report_cb()` in `main.c` currently ignores them.  To support
FFB passthrough, uncomment the `uart_write_blocking()` block and ensure your
GIMX setup handles the feedback packets on the PC side.

### Other GIMX firmwares

The same structure (different VID/PID + different HID descriptor + identical
serial protocol) works for EMUPS4, EMUXONE, EMUDF, etc.  Only
`usb_descriptors.c` and the initial report template in `adapter_common.c`
need changing per target.

> **Xbox / EMUXONE**: Xbox uses a proprietary protocol, not standard HID.
> Use TinyUSB's vendor class (`CFG_TUD_VENDOR 1`) and implement the XInput
> descriptor manually.  See [Drewol/rp2040-gamecon](https://github.com/Drewol/rp2040-gamecon)
> for a working XInput + TinyUSB reference.

---

## License

Derived from [GIMX-firmwares](https://github.com/matlo/GIMX-firmwares) by Mathieu Laurendeau.
Original code licensed GPL-3.0.  This port inherits that license.
