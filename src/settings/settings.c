#include "settings/settings.h"
#include "persistence/persistence.h"
#include "profile/keymap.h"
#include <string.h>

// 設定の状態を表す構造体
typedef struct {
  icode_t dynamic_keymap[KEYMAP_ENTRY_COUNT]; // キーマップ
} settings_state_t;

static settings_state_t settings_state;
static bool persistence_unavailable;

/*
 * 内部関数宣言
 */

static settings_update_result_t _settings_persist_current_state(void);
static settings_update_result_t _settings_rebuild_current_state(void);
static settings_update_result_t _settings_unchanged_result(void);

/*
 * 公開関数
 */

/**
 * @brief 設定を初期化する
 */
void settings_init(void) {
  // 設定状態を初期化する
  memset(&settings_state, 0, sizeof(settings_state));
  persistence_unavailable = false;

  // キーマップを初期化する
  keymap_init();
  keymap_reset(settings_state.dynamic_keymap, KEYMAP_ENTRY_COUNT);

  // 永続化領域を初期化して、保存済みの設定を復元する
  if (!persistence_init(sizeof(settings_state))) {
    persistence_unavailable = true;
    return;
  }
  if (persistence_restore((uint8_t *)&settings_state, sizeof(settings_state))) {
    return;
  }

  // 復元できない場合は設定を初期値に戻して永続化領域を再構築する
  keymap_reset(settings_state.dynamic_keymap, KEYMAP_ENTRY_COUNT);
  if (!persistence_rebuild((const uint8_t *)&settings_state,
                           sizeof(settings_state))) {
    persistence_unavailable = true;
  }
}

/**
 * @brief 設定の永続化イメージのサイズを返す
 * @return 設定の永続化イメージのサイズ
 */
size_t settings_image_size(void) { return sizeof(settings_state); }

/**
 * @brief 指定されたレイヤー、行、列の内部キーコードを取得する
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @return 指定された位置の内部キーコード
 */
icode_t settings_get_keycode(uint8_t layer, uint8_t row, uint8_t col) {
  return keymap_get(settings_state.dynamic_keymap, layer, row, col);
}

/**
 * @brief 指定されたレイヤー、行、列の内部キーコードを設定する
 * @param layer レイヤー番号
 * @param row 行番号
 * @param col 列番号
 * @param keycode 設定する内部キーコード
 * @return 設定の更新結果
 */
settings_update_result_t settings_set_keycode(uint8_t layer, uint8_t row,
                                              uint8_t col, icode_t keycode) {
  if (!keymap_is_valid_position(layer, row, col) ||
      !keymap_is_valid_keycode(keycode)) {
    return SETTINGS_UPDATE_FAILED;
  }

  // 現在のキーコードと同じ場合は変更なしとして扱う
  const icode_t current_keycode =
      keymap_get(settings_state.dynamic_keymap, layer, row, col);
  if (current_keycode == keycode) {
    return _settings_unchanged_result();
  }

  // キーマップを更新して、永続化領域に保存する
  if (!keymap_set(settings_state.dynamic_keymap, layer, row, col, keycode)) {
    return SETTINGS_UPDATE_FAILED;
  }
  return _settings_persist_current_state();
}

/**
 * @brief 指定された設定IDの設定を初期値にリセットする
 * @param id 設定ID
 * @return 設定の更新結果
 */
settings_update_result_t settings_reset(settings_id_t id) {
  if (id != SETTINGS_ID_KEYMAP) {
    return SETTINGS_UPDATE_FAILED;
  }

  // キーマップを初期値にリセットする
  settings_state_t reset_state = settings_state;
  keymap_reset(reset_state.dynamic_keymap, KEYMAP_ENTRY_COUNT);

  // 現在の設定状態と同じ場合は変更なしとして扱う
  if (memcmp(&settings_state, &reset_state, sizeof(settings_state)) == 0) {
    return _settings_unchanged_result();
  }

  // 設定状態を更新して、永続化領域に再構築する
  settings_state = reset_state;
  return _settings_rebuild_current_state();
}

/*
 * 内部関数
 */

/**
 * @brief 現在の設定状態を永続化領域に保存する
 * @return 設定の更新結果
 */
static settings_update_result_t _settings_persist_current_state(void) {
  if (persistence_unavailable) {
    return SETTINGS_UPDATE_RAM_ONLY;
  }

  if (persistence_commit((const uint8_t *)&settings_state,
                         sizeof(settings_state))) {
    return SETTINGS_UPDATE_PERSISTED;
  }

  persistence_unavailable = true;
  return SETTINGS_UPDATE_RAM_ONLY;
}

/**
 * @brief 現在の設定状態を永続化領域に再構築する
 * @return 設定の更新結果
 */
static settings_update_result_t _settings_rebuild_current_state(void) {
  if (persistence_unavailable) {
    return SETTINGS_UPDATE_RAM_ONLY;
  }

  if (persistence_rebuild((const uint8_t *)&settings_state,
                          sizeof(settings_state))) {
    return SETTINGS_UPDATE_PERSISTED;
  }

  persistence_unavailable = true;
  return SETTINGS_UPDATE_RAM_ONLY;
}

/**
 * @brief 設定が変更されなかった場合の更新結果を返す
 * @return 設定の更新結果
 */
static settings_update_result_t _settings_unchanged_result(void) {
  return persistence_unavailable ? SETTINGS_UPDATE_RAM_ONLY
                                 : SETTINGS_UPDATE_PERSISTED;
}
