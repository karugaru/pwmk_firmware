# PWMK CMakeプロジェクト設定

cmake_minimum_required(VERSION 3.13)

# 使用する言語を設定する。
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# プロファイル生成処理が参照する入力ファイルを収集する。
file(GLOB PWMK_TOOL_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tools/*.py"
)
file(GLOB_RECURSE PWMK_PROFILE_GENERATION_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tools/templates/*"
    "${CMAKE_CURRENT_LIST_DIR}/users/*"
)
list(APPEND PWMK_PROFILE_GENERATION_INPUTS
    ${PWMK_TOOL_INPUTS}
    "${CMAKE_CURRENT_LIST_DIR}/pyproject.toml"
    "${CMAKE_CURRENT_LIST_DIR}/uv.lock"
)
# 生成処理の入力をCMakeの再構成トリガーとして登録する。
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PWMK_PROFILE_GENERATION_INPUTS}
)

# ソース生成用コマンドを構築する。
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

# CMakeの構成処理中に、選択中プロファイルの設定とソースを生成する。
execute_process(
    COMMAND ${PWMK_GENERATE_COMMAND}
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMAND_ECHO STDOUT
    RESULT_VARIABLE PWMK_GENERATE_RESULT
)

# プロファイル生成に失敗した場合はビルドを中止する。
if (NOT PWMK_GENERATE_RESULT EQUAL 0)
    message(FATAL_ERROR "pwmk generate failed with exit code ${PWMK_GENERATE_RESULT}")
endif()

# 生成されたプロファイル設定を読み込む。
include(${CMAKE_BINARY_DIR}/generated/profile/profile.cmake)

# 永続化用のIDはファームウェアをビルドするたびに新しく生成する。
set(PWMK_PERSISTENCE_IDENTITY_HEADER
    ${CMAKE_BINARY_DIR}/generated/profile/src/settings/persistence_identity.h
)
add_custom_target(pwmk_profile_generation
    COMMAND ${PWMK_GENERATE_COMMAND}
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    BYPRODUCTS ${PWMK_PERSISTENCE_IDENTITY_HEADER}
    VERBATIM
)

# == Raspberry Pi Pico VS Code拡張機能の動作に必要なため、以下の行は編集しないでください ==
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

# USB HIDとBLEをオプションとして定義する。
option(PWMK_ENABLE_USB "Build USB HID transport" ON)
option(PWMK_ENABLE_BLE "Build BLE transport" ON)

# Raspberry Pi Pico SDKを取り込む。project()より前に実行する必要がある。
include(pico_sdk_import.cmake)

project(pwmk C CXX ASM)

# Raspberry Pi Pico SDKを初期化する。
pico_sdk_init()

# USB HIDを無効にした場合は、stdioの出力先としてUSB CDCを使用する。
if (PWMK_ENABLE_USB)
    set(PWMK_ENABLE_USB_VALUE 1)
    set(PWMK_STDIO_USB_VALUE 0)
else()
    set(PWMK_ENABLE_USB_VALUE 0)
    set(PWMK_STDIO_USB_VALUE 1)
endif()

if (PWMK_ENABLE_BLE)
    set(PWMK_ENABLE_BLE_VALUE 1)

    # BTstack標準のLEデバイスデータベース実装を外し、ローカル実装を使用する。
    get_target_property(PICO_BTSTACK_BLE_SOURCES pico_btstack_ble INTERFACE_SOURCES)
    if (PICO_BTSTACK_BLE_SOURCES)
        list(REMOVE_ITEM PICO_BTSTACK_BLE_SOURCES "${PICO_BTSTACK_PATH}/src/ble/le_device_db_tlv.c")
        set_property(TARGET pico_btstack_ble PROPERTY INTERFACE_SOURCES "${PICO_BTSTACK_BLE_SOURCES}")
    endif()
else()
    set(PWMK_ENABLE_BLE_VALUE 0)
endif()

# コンパイル対象のソースを収集する。
set(PWMK_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS ${PWMK_SOURCE_DIR}/*.c)

list(APPEND SRC_FILES
    ${PWMK_PROFILE_C_SOURCES}
)

# 収集したソースから実行ファイルを作成する。
add_executable(pwmk ${SRC_FILES})
add_dependencies(pwmk pwmk_profile_generation)

# PIOプログラムからCヘッダーを生成する。
pico_generate_pio_header(pwmk ${PWMK_SOURCE_DIR}/led/ws2812.pio OUTPUT_DIR ${PWMK_PROFILE_GENERATED_SRC_DIR}/led)

# 常に使用するPico SDKのライブラリをリンクする。
target_link_libraries(pwmk
    pico_stdlib
    pico_low_power
    hardware_i2c
    hardware_flash
    hardware_pio
    hardware_rosc
    hardware_xosc
    hardware_watchdog
    pico_btstack_flash_bank_headers
    pico_flash
)

# BLEを有効にした場合だけ、BTstackとCYW43関連のライブラリをリンクする。
if (PWMK_ENABLE_BLE)
    target_link_libraries(pwmk
        pico_btstack_ble
        pico_btstack_cyw43
        pico_cyw43_arch_none
    )
endif()

# USBを有効にした場合だけ、TinyUSBと固有ID取得用のライブラリをリンクする。
if (PWMK_ENABLE_USB)
    target_link_libraries(pwmk
        tinyusb_device
        pico_unique_id
    )
endif()

# USBとUARTのstdioを設定する。
pico_enable_stdio_usb(pwmk ${PWMK_STDIO_USB_VALUE})
pico_enable_stdio_uart(pwmk 1)

# ヘッダーをインクルード検索対象に追加する。
target_include_directories(pwmk PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PWMK_SOURCE_DIR}
    ${PWMK_PROFILE_GENERATED_SRC_DIR}
)

# BLEを有効にした場合は、GATT定義からBTstack用のヘッダーを生成する。
if (PWMK_ENABLE_BLE)
    pico_btstack_make_gatt_header(pwmk PRIVATE "${CMAKE_BINARY_DIR}/generated/profile/pwmk.gatt")
endif()

# UF2、BIN、HEX、MAPなどの追加出力を生成する。
pico_add_extra_outputs(pwmk)

# コンパイル時の機能フラグとデバッグ出力の設定を定義する。
target_compile_definitions(pwmk PRIVATE
    PWMK_ENABLE_USB=${PWMK_ENABLE_USB_VALUE}
    PWMK_ENABLE_BLE=${PWMK_ENABLE_BLE_VALUE}
    PICO_FLASH_ASSUME_CORE1_SAFE=1
    #WANT_HCI_DUMP=1 # BTstackのデバッグログを有効にする場合に使用する
    DEBUG_MAIN=0
    DEBUG_BLE=0
    DEBUG_EVENT=0
    DEBUG_MATRIX_SCAN=0
    DEBUG_MATRIX_SCAN_DEEP=0
    DEBUG_PINNACLE=0
    DEBUG_PERIPHERAL=0
    DEBUG_VIAL=0
)

# リンク時にフラッシュ（ROM）とRAMの使用量を表示する。
target_link_options(pwmk PRIVATE -Xlinker --print-memory-usage)

# 必要に応じてコンパイラー警告を有効にする。
# target_compile_options(pwmk PRIVATE -Wall -Wextra)
