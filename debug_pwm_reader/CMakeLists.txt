cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pico SDK setup
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

set(PICO_BOARD pico2 CACHE STRING "Board type")

include(../pico_sdk_import.cmake)

project(debug-pwm-reader C CXX ASM)

pico_sdk_init()

add_executable(debug-pwm-reader main.c)

pico_enable_stdio_uart(debug-pwm-reader 0)
pico_enable_stdio_usb(debug-pwm-reader 1)

target_link_libraries(debug-pwm-reader
    pico_stdlib
    hardware_gpio
    hardware_timer
)

pico_add_extra_outputs(debug-pwm-reader)
