#ifndef PWMK_TEST_KEYMAP_H
#define PWMK_TEST_KEYMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "keyboard/code.h"
#include "profile/board.h"

#define KEYMAP_LAYER_COUNT 1
#define KEYMAP_BUFFER_SIZE (KEYMAP_LAYER_COUNT * ROWS * COLS * 2)
#define PWMK_KEYCODE_SIZE 4u
#define PWMK_KEYMAP_DATA_SIZE \
  (KEYMAP_LAYER_COUNT * ROWS * COLS * PWMK_KEYCODE_SIZE)

#define KEYMAP { IKC_A, LEFT_SHIFT(IKC_B), ICC_VOL_UP, IKC_NOOP }

void keymap_init(void);
void keymap_reset(void);
icode_t keymap_get(uint8_t layer, uint8_t row, uint8_t col);
bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, icode_t keycode);
bool keymap_is_valid_keycode(icode_t keycode);
bool keymap_export_pwmk(uint8_t *buffer, size_t size);
bool keymap_import_pwmk(const uint8_t *buffer, size_t size);
bool keymap_get_pwmk_offset(uint8_t layer, uint8_t row, uint8_t col,
                            size_t *offset);

#endif // PWMK_TEST_KEYMAP_H
