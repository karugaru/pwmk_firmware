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
