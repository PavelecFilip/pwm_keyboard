cmake_minimum_required(VERSION 3.13)

# Zkus PICO_SDK_PATH z prostředí, jinak fallback na výchozí cestu
if(DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
elseif(EXISTS "$ENV{HOME}/pico/pico-sdk")
    set(PICO_SDK_PATH "$ENV{HOME}/pico/pico-sdk")
else()
    message(FATAL_ERROR "PICO_SDK_PATH není nastaveno. Nastav ho přes: export PICO_SDK_PATH=/cesta/k/pico-sdk")
endif()
message(STATUS "PICO_SDK_PATH is ${PICO_SDK_PATH}")

include(pico_sdk_import.cmake)

project(pwm_keyboard C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pwm_keyboard
    main.c
    ssd1306.c
)

target_link_libraries(pwm_keyboard
    pico_stdlib
    hardware_pwm
    hardware_gpio
    hardware_i2c
)

pico_add_extra_outputs(pwm_keyboard)
pico_enable_stdio_usb(pwm_keyboard 1)
pico_enable_stdio_uart(pwm_keyboard 0)
