#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "keyboard/code_convert.h"
#include "profile/board.h"
#include "profile/keymap.h"

#define KEYMAP_ENTRIES_PER_LAYER (ROWS * COLS)
#define KEYMAP_ENTRY_COUNT (KEYMAP_LAYER_COUNT * KEYMAP_ENTRIES_PER_LAYER)

// 内部キーコードの既定キーマップ。
static const icode_t keymap[KEYMAP_ENTRY_COUNT] = KEYMAP;

// 内部キーコードの動的キーマップ。
static icode_t dynamic_keymap[KEYMAP_ENTRY_COUNT];

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
 * @brief キーマップを初期化する。
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

  keymap_reset();
}

/**
 * @brief 動的キーマップを既定のキーマップにリセットする。
 */
void keymap_reset(void) {
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
 * @brief
 * キーマップの指定した位置の内部用キーコードを取得する。
 * 指定した位置にキーが存在しない場合はIKC_NOOPを返す。
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @return 内部用キーコード
 */
icode_t keymap_get(uint8_t layer, uint8_t row, uint8_t col) {
  if (layer >= KEYMAP_LAYER_COUNT || row >= ROWS || col >= COLS) {
    return IKC_NOOP;
  }

  const int16_t index = keyswitch_index_lookup[row][col];
  if (index != -1) {
    return dynamic_keymap[layer * KEYMAP_ENTRIES_PER_LAYER + index];
  }
  return IKC_NOOP;
}

/**
 * @brief キーマップの指定した位置に内部用キーコードを設定する。
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

  const int16_t index = keyswitch_index_lookup[row][col];
  if (index == -1) {
    return false;
  }

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

/**
 * @brief RAM上のキーマップをPWMK形式へ直列化する。
 * @param buffer 直列化先のバッファ
 * @param size バッファサイズ
 * @return 直列化に成功した場合はtrue、失敗した場合はfalse
 */
bool keymap_export_pwmk(uint8_t *buffer, size_t size) {
  if (buffer == NULL || size != PWMK_KEYMAP_DATA_SIZE) {
    return false;
  }

  for (size_t index = 0; index < KEYMAP_ENTRY_COUNT; index++) {
    const size_t layout_index = index % KEYMAP_ENTRIES_PER_LAYER;
    uint32_t value;
    if (_keymap_layout_index_exists(layout_index)) {
      value = (uint32_t)dynamic_keymap[index];
    } else {
      value = IKC_NOOP;
    }
    const size_t offset = index * PWMK_KEYCODE_SIZE;
    buffer[offset] = (uint8_t)value;
    buffer[offset + 1u] = (uint8_t)(value >> 8u);
    buffer[offset + 2u] = (uint8_t)(value >> 16u);
    buffer[offset + 3u] = (uint8_t)(value >> 24u);
  }
  return true;
}

/**
 * @brief PWMK形式のキーマップを検証してRAMへ反映する。
 * @param buffer PWMK形式のデータ
 * @param size バッファサイズ
 * @return 読み込みに成功した場合はtrue、失敗した場合はfalse
 */
bool keymap_import_pwmk(const uint8_t *buffer, size_t size) {
  if (buffer == NULL || size != PWMK_KEYMAP_DATA_SIZE) {
    return false;
  }

  icode_t imported_keymap[KEYMAP_ENTRY_COUNT];
  for (size_t index = 0; index < KEYMAP_ENTRY_COUNT; index++) {
    const size_t layout_index = index % KEYMAP_ENTRIES_PER_LAYER;
    const size_t offset = index * PWMK_KEYCODE_SIZE;
    const uint32_t value = (uint32_t)buffer[offset] |
                           ((uint32_t)buffer[offset + 1u] << 8u) |
                           ((uint32_t)buffer[offset + 2u] << 16u) |
                           ((uint32_t)buffer[offset + 3u] << 24u);
    if ((!_keymap_layout_index_exists(layout_index) && value != IKC_NOOP) ||
        !keymap_is_valid_keycode((icode_t)value)) {
      return false;
    }
    imported_keymap[index] = (icode_t)value;
  }

  memcpy(dynamic_keymap, imported_keymap, sizeof(dynamic_keymap));
  return true;
}

/**
 * @brief 行列位置に対応するPWMK形式データ内の相対オフセットを返す。
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @param offset 相対オフセットの格納先
 * @return 位置が存在する場合はtrue、そうでない場合はfalse
 */
bool keymap_get_pwmk_offset(uint8_t layer, uint8_t row, uint8_t col,
                            size_t *offset) {
  if (offset == NULL || layer >= KEYMAP_LAYER_COUNT || row >= ROWS ||
      col >= COLS) {
    return false;
  }

  const int16_t index = keyswitch_index_lookup[row][col];
  if (index == -1) {
    return false;
  }

  *offset = ((size_t)layer * KEYMAP_ENTRIES_PER_LAYER + (size_t)index) *
            PWMK_KEYCODE_SIZE;
  return true;
}
