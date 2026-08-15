#ifndef PWMK_SETTINGS_H
#define PWMK_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "keyboard/code.h"
#include "profile/settings.h"

void settings_init(void);
icode_t settings_get_keycode(uint8_t layer, uint8_t row, uint8_t col);
bool settings_set_keycode(uint8_t layer, uint8_t row, uint8_t col,
                          icode_t keycode);
bool settings_reset_keymap(void);

#endif // PWMK_SETTINGS_H
