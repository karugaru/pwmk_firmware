#include <string.h>

#include "keyboard/code_convert.h"
#include "settings/board.h"
#include "settings/keymap.h"

// 内部キーコードの既定キーマップ。
static const icode_t keymap[ROWS * COLS] = KEYMAP;

// 内部キーコードの動的キーマップ。
static icode_t dynamic_keymap[ROWS * COLS];

// 行番号と列番号からキーインデックスを取得するためのルックアップテーブル。
// インデックスが-1の場合はその行列位置に対応するキーが存在しないことを示す。
// 行番号と列番号は回路的な配置を示すもので、キーの物理的な配置とは異なる場合がある。
static int16_t keyswitch_index_lookup[ROWS][COLS];

/**
 * @brief
 * キーマップを初期化する。
 */
void keymap_init(void) {
  // ルックアップテーブルを初期化
  for (uint8_t row = 0; row < ROWS; row++) {
    for (uint8_t col = 0; col < COLS; col++) {
      keyswitch_index_lookup[row][col] = -1;
    }
  }

  for (uint16_t index = 0; index < ROWS * COLS; index++) {
    int8_t row = layout[index][0];
    int8_t col = layout[index][1];
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS) {
      continue;
    }
    keyswitch_index_lookup[row][col] = index;
  }

  keymap_reset();
}

/**
 * @brief
 * 動的キーマップを既定のキーマップにリセットする。
 */
void keymap_reset(void) {
  memset(dynamic_keymap, 0, sizeof(dynamic_keymap));

  for (uint16_t index = 0; index < ROWS * COLS; index++) {
    dynamic_keymap[index] = keymap[index];
  }
}

/**
 * @brief
 * キーマップの指定した位置の内部用キーコードを取得する。
 * 指定した位置にキーが存在しない場合はIKC_NOOPを返す。
 *
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @return 内部用キーコード
 */
icode_t keymap_get(uint8_t layer, uint8_t row, uint8_t col) {
  if (layer >= KEYMAP_LAYER_COUNT || row >= ROWS || col >= COLS) {
    return IKC_NOOP;
  }

  int16_t index = keyswitch_index_lookup[row][col];
  if (index != -1) {
    return dynamic_keymap[index];
  }
  return IKC_NOOP;
}

/**
 * @brief
 * キーマップの指定した位置に内部用キーコードを設定する。
 *
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @param keycode 内部用キーコード
 * @return 設定に成功した場合はtrue、失敗した場合はfalse
 */
bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, icode_t keycode) {
  if (layer >= KEYMAP_LAYER_COUNT || row >= ROWS || col >= COLS) {
    return false;
  }

  int16_t index = keyswitch_index_lookup[row][col];
  if (index == -1) {
    return false;
  }

  dynamic_keymap[index] = keycode;
  return true;
}
