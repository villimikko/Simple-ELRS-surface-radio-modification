# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(elrs-surface-radio C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(elrs-surface-radio
    src/main.c
    src/crsf.c
    src/adc.c
    src/led.c
    src/mixer.c
    src/shared.c
    src/screen.c
    src/buttons.c
    src/unused/display.c
    # Unused in KISS mode — moved to src/unused/
    # src/unused/config.c
    # src/unused/ina219.c
    # src/unused/oled.c
    # src/unused/encoder.c
    # src/unused/calibrate.c
    # src/unused/menu.c
    # src/unused/pages.c
)

# Generate PIO header for WS2812 LED driver
pico_generate_pio_header(elrs-surface-radio ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

pico_set_program_name(elrs-surface-radio "ELRS Surface Radio")
pico_set_program_version(elrs-surface-radio "0.2")

# USB stdio for debug output, UART0 is used for CRSF
pico_enable_stdio_uart(elrs-surface-radio 0)
pico_enable_stdio_usb(elrs-surface-radio 1)

target_link_libraries(elrs-surface-radio
    pico_stdlib
    pico_multicore
    hardware_uart
    hardware_adc
    hardware_spi
    hardware_dma
    hardware_gpio
    hardware_pwm
    hardware_timer
    hardware_i2c
    hardware_flash
    hardware_sync
    hardware_pio
)

target_include_directories(elrs-surface-radio PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

pico_add_extra_outputs(elrs-surface-radio)
