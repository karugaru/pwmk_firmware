# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(PWMK_GENERATE_COMMAND
        ${CMAKE_COMMAND} -E env UV_PROJECT_ENVIRONMENT=.venv_win
        uv run tools/pwmk.py generate --build-dir ${CMAKE_BINARY_DIR}
    )
else()
    set(PWMK_GENERATE_COMMAND
        uv run tools/pwmk.py generate --build-dir ${CMAKE_BINARY_DIR}
    )
endif()

execute_process(
    COMMAND ${PWMK_GENERATE_COMMAND}
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND_ECHO STDOUT
    RESULT_VARIABLE PWMK_GENERATE_RESULT
)

if (NOT PWMK_GENERATE_RESULT EQUAL 0)
    message(FATAL_ERROR "pwmk generate failed with exit code ${PWMK_GENERATE_RESULT}")
endif()

include(${CMAKE_BINARY_DIR}/generated/profile/profile.cmake)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

option(PWMK_ENABLE_USB "Build USB HID transport" ON)
option(PWMK_ENABLE_BLE "Build BLE transport" ON)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pwmk C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

if (PWMK_ENABLE_USB)
    set(PWMK_ENABLE_USB_VALUE 1)
    set(PWMK_STDIO_USB_VALUE 0)
else()
    set(PWMK_ENABLE_USB_VALUE 0)
    set(PWMK_STDIO_USB_VALUE 1)
endif()

if (PWMK_ENABLE_BLE)
    set(PWMK_ENABLE_BLE_VALUE 1)

    # Override BTstack's default LE device DB implementation with our local one.
    get_target_property(PICO_BTSTACK_BLE_SOURCES pico_btstack_ble INTERFACE_SOURCES)
    if (PICO_BTSTACK_BLE_SOURCES)
        list(REMOVE_ITEM PICO_BTSTACK_BLE_SOURCES "${PICO_BTSTACK_PATH}/src/ble/le_device_db_tlv.c")
        set_property(TARGET pico_btstack_ble PROPERTY INTERFACE_SOURCES "${PICO_BTSTACK_BLE_SOURCES}")
    endif()
else()
    set(PWMK_ENABLE_BLE_VALUE 0)
endif()

# Add executable. Default name is the project name, version 0.1
set(PWMK_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS ${PWMK_SOURCE_DIR}/*.c)
file(GLOB_RECURSE SETTINGS_SRCS CONFIGURE_DEPENDS "${PWMK_PROFILE_SETTINGS_DIR}/*.c")

list(APPEND SRC_FILES
    ${SETTINGS_SRCS}
    ${PWMK_PROFILE_C_SOURCES}
)

add_executable(pwmk ${SRC_FILES})

pico_generate_pio_header(pwmk ${PWMK_SOURCE_DIR}/led/ws2812.pio OUTPUT_DIR ${PWMK_PROFILE_GENERATED_SRC_DIR}/led)

# pull in common dependencies
target_link_libraries(pwmk
    pico_stdlib
    pico_low_power
    hardware_i2c
    hardware_pio
    hardware_rosc
    hardware_xosc
    hardware_watchdog
)

if (PWMK_ENABLE_BLE)
    target_link_libraries(pwmk
        pico_btstack_ble
        pico_btstack_cyw43
        pico_cyw43_arch_none
    )
endif()

if (PWMK_ENABLE_USB)
    target_link_libraries(pwmk
        tinyusb_device
        pico_unique_id
    )
endif()

pico_enable_stdio_usb(pwmk ${PWMK_STDIO_USB_VALUE})
pico_enable_stdio_uart(pwmk 1)

target_include_directories(pwmk PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PWMK_SOURCE_DIR}
    ${PWMK_PROFILE_GENERATED_SRC_DIR}
)

if (PWMK_ENABLE_BLE)
    pico_btstack_make_gatt_header(pwmk PRIVATE "${CMAKE_BINARY_DIR}/generated/profile/pwmk.gatt")
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(pwmk)

target_compile_definitions(pwmk PRIVATE
    PWMK_ENABLE_USB=${PWMK_ENABLE_USB_VALUE}
    PWMK_ENABLE_BLE=${PWMK_ENABLE_BLE_VALUE}
    #WANT_HCI_DUMP=1 # This enables btstack debug
    DEBUG_MAIN=0
    DEBUG_BLE=0
    DEBUG_EVENT=0
    DEBUG_MATRIX_SCAN=0
    DEBUG_MATRIX_SCAN_DEEP=0
    DEBUG_PINNACLE=0
    DEBUG_PERIPHERAL=0
    DEBUG_VIAL=0
)

# ROM and RAM utilization set to display at build time
target_link_options(pwmk PRIVATE -Xlinker --print-memory-usage)

# Set warnings to display at build time
# target_compile_options(pwmk PRIVATE -Wall -Wextra)
