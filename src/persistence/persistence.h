#ifndef PWMK_PERSISTENCE_H
#define PWMK_PERSISTENCE_H

#include "keyboard/code.h"
#include <stdbool.h>
#include <stdint.h>

bool persistence_init(void);
bool persistence_set_keycode(uint8_t layer, uint8_t row, uint8_t col,
                             icode_t keycode);
bool persistence_reset_keymap(void);
bool persistence_is_available(void);

#endif // PWMK_PERSISTENCE_H
