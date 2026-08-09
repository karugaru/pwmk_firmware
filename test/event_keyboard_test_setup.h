#ifndef PWMK_EVENT_KEYBOARD_TEST_SETUP_H
#define PWMK_EVENT_KEYBOARD_TEST_SETUP_H

#include "keyboard/code.h"
#include <stdbool.h>

void event_keyboard_test_prepare(icode_t keycode);
void event_keyboard_test_set_keycode(icode_t keycode);
bool event_keyboard_test_pop_platform_event(icode_t *keycode, bool *pressed);

#endif // PWMK_EVENT_KEYBOARD_TEST_SETUP_H
