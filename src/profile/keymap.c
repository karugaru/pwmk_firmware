#include "profile/keymap.h"
#include "profile/board.h"
#include <stddef.h>
#include <stdint.h>

#define KEYMAP_ENTRIES_PER_LAYER (ROWS * COLS)

// 内部キーコードの既定キーマップ。
static const icode_t keymap[KEYMAP_ENTRY_COUNT] = KEYMAP;

// 行番号と列番号からキーインデックスを取得するためのルックアップテーブル。
// インデックスが-1の場合はその行列位置に対応するキーが存在しないことを示す。
// 行番号と列番号は回路的な配置を示すもので、キーの物理的な配置とは異なる場合がある。
static int16_t keyswitch_index_lookup[ROWS][COLS];

/**
 * @brief
 * 指定されたレイアウトインデックスに対応するキーが存在するかどうかを返す。
 * @param index レイアウトインデックス
 * @return キーが存在する場合はtrue、そうでない場合はfalse
 */
static bool _keymap_layout_index_exists(size_t index) {
  const int8_t row = layout[index][0];
  const int8_t col = layout[index][1];
  return row >= 0 && row < ROWS && col >= 0 && col < COLS;
}

/**
 * @brief キーマップ関連機能を初期化する。
 */
void keymap_init(void) {
  // ルックアップテーブルを初期化
  for (uint8_t row = 0; row < ROWS; row++) {
    for (uint8_t col = 0; col < COLS; col++) {
      keyswitch_index_lookup[row][col] = -1;
    }
  }

  for (uint16_t index = 0; index < KEYMAP_ENTRIES_PER_LAYER; index++) {
    if (!_keymap_layout_index_exists(index)) {
      continue;
    }
    const int8_t row = layout[index][0];
    const int8_t col = layout[index][1];
    keyswitch_index_lookup[row][col] = index;
  }
}

/**
 * @brief 動的キーマップを既定のキーマップにリセットする。
 * @param dynamic_keymap リセット対象のキーマップ
 * @param keymap_size キーマップの要素数
 */
void keymap_reset(icode_t *dynamic_keymap, size_t keymap_size) {
  if (dynamic_keymap == NULL || keymap_size != KEYMAP_ENTRY_COUNT) {
    return;
  }

  for (uint16_t index = 0; index < KEYMAP_ENTRY_COUNT; index++) {
    // レイアウトのインデックスとキーマップのインデックスは
    // 意味が異なるので注意。keymap_layout_index_existsは
    // レイアウトのインデックスを使ってキーが存在するかを判定する。
    const size_t layout_index = index % KEYMAP_ENTRIES_PER_LAYER;
    if (_keymap_layout_index_exists(layout_index)) {
      dynamic_keymap[index] = keymap[index];
    } else {
      dynamic_keymap[index] = IKC_NOOP;
    }
  }
}

/**
 * @brief 指定された位置にキーが存在するかどうかを判定する。
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @return キーが存在する場合はtrue、そうでない場合はfalse
 */
bool keymap_is_valid_position(uint8_t layer, uint8_t row, uint8_t col) {
  return layer < KEYMAP_LAYER_COUNT && row < ROWS && col < COLS &&
         keyswitch_index_lookup[row][col] != -1;
}

/**
 * @brief
 * 指定された配列からキーコードを取得する。
 * 指定された位置にキーが存在しない場合はIKC_NOOPを返す。
 * @param keymap キーマップ配列
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @return 内部キーコード
 */
icode_t keymap_get(const icode_t *dynamic_keymap, uint8_t layer, uint8_t row,
                   uint8_t col) {
  if (dynamic_keymap == NULL || !keymap_is_valid_position(layer, row, col)) {
    return IKC_NOOP;
  }

  const int16_t index = keyswitch_index_lookup[row][col];
  return dynamic_keymap[layer * KEYMAP_ENTRIES_PER_LAYER + index];
}

/**
 * @brief 配列の指定位置に内部キーコードを設定する。
 * @param dynamic_keymap キーマップ配列
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @param keycode 内部キーコード
 * @return 設定に成功した場合はtrue、失敗した場合はfalse
 */
bool keymap_set(icode_t *dynamic_keymap, uint8_t layer, uint8_t row,
                uint8_t col, icode_t keycode) {
  if (dynamic_keymap == NULL || !keymap_is_valid_position(layer, row, col)) {
    return false;
  }

  const int16_t index = keyswitch_index_lookup[row][col];
  dynamic_keymap[layer * KEYMAP_ENTRIES_PER_LAYER + index] = keycode;
  return true;
}

/**
 * @brief 内部キーコードとして保存可能な値かどうかを返す。
 * @param keycode 判定する内部キーコード
 * @return 保存可能な場合はtrue、そうでない場合はfalse
 */
bool keymap_is_valid_keycode(icode_t keycode) {
  const uint32_t value = (uint32_t)keycode;

  if (value <= UINT16_MAX) {
    const uint8_t base = (uint8_t)value;
    if (base == IKC_NOOP) {
      return value == IKC_NOOP;
    }
    return (base >= IKC_A && base <= IKC_EXSEL) ||
           (base >= IKC_KEYPAD_00 && base <= IKC_KEYPAD_HEXADECIMAL) ||
           (base >= IMKC_LEFT_CONTROL && base <= IMKC_RIGHT_GUI);
  }

  return (value >= ICC_RECORD && value <= ISC_BLE_SLOT_4) ||
         (value >= IUC_RANGE_MIN && value <= IUC_RANGE_MAX);
}
