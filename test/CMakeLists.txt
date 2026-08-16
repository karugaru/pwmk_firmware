cmake_minimum_required(VERSION 3.13)

project(pwmk_keyboard_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_executable(event_keyboard_test
    event_keyboard_test.c
    event_keyboard_test_setup.c
    unity.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/keyboard/event.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/keyboard/code.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/hid/hid.c
)

target_include_directories(event_keyboard_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../src
)

add_test(NAME event_keyboard_test COMMAND event_keyboard_test)

add_executable(code_convert_test
    code_convert_test.c
    unity.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/keyboard/code_convert.c
)

target_include_directories(code_convert_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../src
)

add_test(NAME code_convert_test COMMAND code_convert_test)

add_executable(persistence_format_test
    persistence_format_test.c
    unity.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/persistence/persistence_format.c
)

target_include_directories(persistence_format_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../src
)

add_test(NAME persistence_format_test COMMAND persistence_format_test)

add_executable(keymap_persistence_test
    keymap_persistence_test.c
    unity.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/profile/keymap.c
)

target_include_directories(keymap_persistence_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../src
)

add_test(NAME keymap_persistence_test COMMAND keymap_persistence_test)

add_executable(settings_test
    settings_test.c
    unity.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/settings/settings.c
    ${CMAKE_CURRENT_LIST_DIR}/../src/profile/keymap.c
)

target_include_directories(settings_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../src
)

add_test(NAME settings_test COMMAND settings_test)
