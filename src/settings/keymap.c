#include <string.h>

#include "settings/board.h"
#include "settings/keymap.h"

static const icode_t keymap[ROWS * COLS] = KEYMAP;
static size_t keyswitch_index_lookup[ROWS][COLS];
static icode_t dynamic_keymap[ROWS][COLS];

static void keymap_vial_reset_internal(void) {
  memset(dynamic_keymap, 0, sizeof(dynamic_keymap));

  for (size_t index = 0; index < ROWS * COLS; index++) {
    uint8_t row = layout[index][0];
    uint8_t col = layout[index][1];
    if (row == (uint8_t)-1 || col == (uint8_t)-1) {
      continue;
    }
    dynamic_keymap[row][col] = keymap[index];
  }
}

void keyswitch_index_init(void) {
  for (uint8_t row = 0; row < ROWS; row++) {
    for (uint8_t col = 0; col < COLS; col++) {
      keyswitch_index_lookup[row][col] = -1;
    }
  }

  for (size_t index = 0; index < ROWS * COLS; index++) {
    uint8_t row = layout[index][0];
    uint8_t col = layout[index][1];
    if (row == (uint8_t)-1 || col == (uint8_t)-1) {
      continue;
    }
    keyswitch_index_lookup[row][col] = index;
  }

  keymap_vial_reset_internal();
}

icode_t icode_lookup(uint8_t row, uint8_t col) {
  if (row >= ROWS || col >= COLS) {
    return IKC_NOOP;
  }

  size_t index = keyswitch_index_lookup[row][col];
  if (index != -1) {
    return dynamic_keymap[row][col];
  }
  return IKC_NOOP;
}

void keymap_vial_reset(void) { keymap_vial_reset_internal(); }

uint16_t keymap_vial_get(uint8_t layer, uint8_t row, uint8_t col) {
  if (layer != 0 || row >= ROWS || col >= COLS) {
    return IKC_NOOP;
  }

  icode_t keycode = dynamic_keymap[row][col];
  if (keycode == IKC_NOOP || (keycode >= IKC_A && keycode <= IMKC_RIGHT_GUI)) {
    return (uint16_t)keycode;
  }
  return IKC_NOOP;
}

bool keymap_vial_is_supported_keycode(uint16_t keycode) {
  return keycode == IKC_NOOP || (keycode >= IKC_A && keycode <= IMKC_RIGHT_GUI);
}

bool keymap_vial_set(uint8_t layer, uint8_t row, uint8_t col,
                     uint16_t keycode) {
  if (layer != 0 || row >= ROWS || col >= COLS) {
    return false;
  }
  if (!keymap_vial_is_supported_keycode(keycode)) {
    return false;
  }

  dynamic_keymap[row][col] = (icode_t)keycode;
  return true;
}
