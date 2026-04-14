cmake_minimum_required(VERSION 3.13)

# Pull in Pico SDK (must be before project())
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(emups3 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(emups3
    src/main.c
    src/adapter_common.c
    src/usb_descriptors.c
)

# Point TinyUSB at our config header
target_include_directories(emups3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(emups3
    pico_stdlib
    pico_unique_id
    tinyusb_device
    tinyusb_board
    hardware_uart
    hardware_gpio
)

# Disable USB stdio so TinyUSB owns the USB peripheral
pico_enable_stdio_usb(emups3 0)
pico_enable_stdio_uart(emups3 0)   # debug UART disabled; UART0 is used for GIMX comms

# Generate .uf2 / .bin / .hex
pico_add_extra_outputs(emups3)
