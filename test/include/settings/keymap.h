#ifndef PWMK_TEST_KEYMAP_H
#define PWMK_TEST_KEYMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "keyboard/code.h"
#include "profile/board.h"

#define KEYMAP_LAYER_COUNT 1
#define KEYMAP_ENTRY_COUNT (KEYMAP_LAYER_COUNT * ROWS * COLS)
#define KEYMAP_BUFFER_SIZE (KEYMAP_ENTRY_COUNT * 2)

#define KEYMAP { IKC_A, LEFT_SHIFT(IKC_B), ICC_VOL_UP, IKC_NOOP }

void keymap_init(void);
void keymap_reset(icode_t *keymap, size_t keymap_size);
bool keymap_is_valid_position(uint8_t layer, uint8_t row, uint8_t col);
icode_t keymap_get(const icode_t *keymap, uint8_t layer, uint8_t row,
                  uint8_t col);
bool keymap_set(icode_t *keymap, uint8_t layer, uint8_t row, uint8_t col,
                icode_t keycode);
bool keymap_is_valid_keycode(icode_t keycode);

#endif // PWMK_TEST_KEYMAP_H
