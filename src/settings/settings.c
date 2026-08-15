#include "settings/settings.h"
#include "persistence/persistence.h"
#include "profile/keymap.h"

/**
 * @brief
 * プロファイル設定に必要な初期化を行う。
 */
void settings_init(void) {
  keymap_init();
  persistence_init();
}

icode_t settings_get_keycode(uint8_t layer, uint8_t row, uint8_t col) {
  return keymap_get(layer, row, col);
}

bool settings_set_keycode(uint8_t layer, uint8_t row, uint8_t col,
                          icode_t keycode) {
  return persistence_set_keycode(layer, row, col, keycode);
}

bool settings_reset_keymap(void) { return persistence_reset_keymap(); }
