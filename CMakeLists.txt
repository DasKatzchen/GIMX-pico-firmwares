cmake_minimum_required(VERSION 3.13)

# ---------------------------------------------------------------------------
# Pico-PIO-USB must be cloned alongside this project:
#   git clone https://github.com/sekigon-gonnoc/Pico-PIO-USB.git
# Set PICO_PIO_USB_PATH or it defaults to ../Pico-PIO-USB
# ---------------------------------------------------------------------------
if(NOT DEFINED PICO_PIO_USB_PATH)
    set(PICO_PIO_USB_PATH ${CMAKE_CURRENT_LIST_DIR}/../Pico-PIO-USB)
endif()

# Pull in Pico SDK (must be before project())
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(emups3 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ---------------------------------------------------------------------------
# Pico-PIO-USB library
# ---------------------------------------------------------------------------
add_subdirectory(${PICO_PIO_USB_PATH} pico_pio_usb)

# ---------------------------------------------------------------------------
# Main target
# ---------------------------------------------------------------------------
add_executable(emups3
    src/main.c
    src/adapter_common.c
    src/usb_descriptors.c
    src/ring_buffer.c
)

target_include_directories(emups3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PICO_PIO_USB_PATH}/src         # pio_usb.h
)

target_link_libraries(emups3
    pico_stdlib
    pico_multicore           # multicore_launch_core1 etc.
    pico_unique_id
    tinyusb_device           # native USB → PS3 (HID device)
    tinyusb_host             # PIO USB  → PC  (CDC host)
    tinyusb_board
    pico_pio_usb             # Pico-PIO-USB library
    hardware_pio
    hardware_dma
)

# No USB stdio — TinyUSB owns the USB peripheral
pico_enable_stdio_usb(emups3 0)
# UART0 pins GP0/GP1 available for debug printf if needed
pico_enable_stdio_uart(emups3 1)

pico_add_extra_outputs(emups3)
