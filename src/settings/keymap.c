#include <string.h>

#include "keyboard/code_convert.h"
#include "settings/board.h"
#include "settings/keymap.h"

// 内部キーコードの既定キーマップ。
static const icode_t keymap[ROWS * COLS] = KEYMAP;
// 行番号と列番号からキーインデックスを取得するためのルックアップテーブル。
static int16_t keyswitch_index_lookup[ROWS][COLS];
// 内部キーコードの動的キーマップ。
static icode_t dynamic_keymap[ROWS][COLS];

/**
 * @brief
 * 動的キーマップを既定値で初期化する。
 */
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

/**
 * @brief
 * キーインデックスルックアップテーブルを初期化する。
 */
void keymap_index_init(void) {
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

/**
 * @brief
 * 行番号と列番号から内部用キーコードを取得する。
 *
 * @param row 行番号
 * @param col 列番号
 * @return 内部用キーコード
 */
icode_t keymap_icode_lookup(uint8_t row, uint8_t col) {
  if (row >= ROWS || col >= COLS) {
    return IKC_NOOP;
  }

  int16_t index = keyswitch_index_lookup[row][col];
  if (index != -1) {
    return dynamic_keymap[row][col];
  }
  return IKC_NOOP;
}

/**
 * @brief
 * 動的キーマップを既定値で初期化する。
 */
void keymap_vial_reset(void) { keymap_vial_reset_internal(); }

/**
 * @brief
 * 動的キーマップからVIALのキーコードを取得する。
 *
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @return VIALのキーコード
 */
uint16_t keymap_vial_get(uint8_t layer, uint8_t row, uint8_t col) {
  if (layer != 0 || row >= ROWS || col >= COLS) {
    return IKC_NOOP;
  }

  icode_t keycode_internal = dynamic_keymap[row][col];

  uint16_t keycode_vial;
  if (!code_convert_to_vial(keycode_internal, &keycode_vial)) {
    return IKC_NOOP;
  }

  return keycode_vial;
}

/**
 * @brief
 * 動的キーマップの指定した位置にVIALのキーコードを設定する。
 *
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @param keycode_vial VIALのキーコード
 * @return 設定に成功した場合はtrue、失敗した場合はfalse
 */
bool keymap_vial_set(uint8_t layer, uint8_t row, uint8_t col,
                     uint16_t keycode_vial) {
  if (layer != 0 || row >= ROWS || col >= COLS) {
    return false;
  }

  icode_t keycode_internal;
  if (!code_convert_to_internal(keycode_vial, &keycode_internal)) {
    return false;
  }

  dynamic_keymap[row][col] = keycode_internal;
  return true;
}
