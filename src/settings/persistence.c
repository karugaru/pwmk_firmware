#include "settings/persistence.h"
#include "settings/keymap.h"
#include "settings/persistence_flash.h"
#include "settings/persistence_format.h"
#include "settings/persistence_identity.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PWMK_BUILT_DATA_SIZE PWMK_KEYMAP_DATA_SIZE

// 永続化の状態を保持する構造体
typedef struct {
  bool available;              // 永続化領域が有効かどうか
  size_t completed_save_count; // 完了済みの保存回数
  size_t log_used_size;        // 使用済みのログ領域サイズ
} persistence_state_t;

static persistence_state_t persistence_state;

/**
 * @brief 保存進捗を更新する
 * @param slot 更新する保存進捗スロット番号
 * @param mark_completed 完了済みにするかどうか
 */
static bool _persistence_update_progress(size_t slot, bool mark_completed) {
  size_t byte_offset;
  uint8_t clear_mask;

  // スロットの更新に必要な情報を取得する
  if (!persistence_format_progress_get_slot_update(slot, mark_completed,
                                                   &byte_offset, &clear_mask)) {
    return false;
  }

  // 現在のスロットの値をフラッシュから読み込む
  size_t offset = persistence_flash_layout.progress_offset + byte_offset;
  uint8_t current_value;
  if (!persistence_flash_read(offset, &current_value, sizeof(current_value))) {
    return false;
  }

  // 更新用の値を計算する
  uint8_t updated_value;
  if (!persistence_format_progress_update_slot(
          current_value, slot, mark_completed, &updated_value)) {
    return false;
  }

  // フラッシュに更新後の値を書き込む
  return persistence_flash_program(offset, &updated_value,
                                   sizeof(updated_value));
}

/**
 * @brief 保存進捗を読み込む
 * @param info 進捗情報を格納する構造体へのポインタ
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
static bool _persistence_read_progress(pwmk_progress_info_t *info) {
  uint8_t progress[PWMK_PROGRESS_SIZE];

  // フラッシュから保存進捗を読み込む
  if (!persistence_flash_read(persistence_flash_layout.progress_offset,
                              progress, sizeof(progress))) {
    return false;
  }

  // 読み込んだ保存進捗を解析する
  return persistence_format_progress_analyze(progress, sizeof(progress), info);
}

/**
 * @brief
 * 永続化領域を初期化する。RAM上のキーマップか、規定のキーマップを使用して永続化領域を初期化する。
 * @param reset_keymap 規定のキーマップを使用するかどうか
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
static bool _persistence_rebuild(bool reset_keymap) {
  persistence_state.available = false;

  // フラッシュ領域を消去する
  if (!persistence_flash_erase_all()) {
    return false;
  }
  // マーカーを書き込む
  if (!persistence_flash_program(persistence_flash_layout.marker_offset,
                                 (const uint8_t *)PWMK_PERSISTENCE_MARKER,
                                 PWMK_PERSISTENCE_MARKER_SIZE)) {
    return false;
  }
  // シリアル番号を書き込む
  if (!persistence_flash_program(persistence_flash_layout.serial_offset,
                                 pwmk_firmware_serial,
                                 PWMK_FIRMWARE_SERIAL_SIZE)) {
    return false;
  }
  // 保存進捗を保存中にする
  if (!_persistence_update_progress(0u, false)) {
    return false;
  }

  // 規定のキーマップを使用する場合、RAM上のキーマップをリセットする
  if (reset_keymap) {
    keymap_reset();
  }

  // RAM上のキーマップを構築済みデータに変換する
  uint8_t built_data[PWMK_BUILT_DATA_SIZE];
  if (!keymap_export_pwmk(built_data, sizeof(built_data))) {
    return false;
  }
  // 構築済みデータを書き込む
  if (!persistence_flash_program(persistence_flash_layout.built_data_offset,
                                 built_data, sizeof(built_data))) {
    return false;
  }
  // 保存進捗を保存完了にする
  if (!_persistence_update_progress(0u, true)) {
    return false;
  }
  // 保存進捗を読み込んで、正しく初期化されたか確認する
  pwmk_progress_info_t progress_info;
  if (!_persistence_read_progress(&progress_info) ||
      progress_info.state != PWMK_PROGRESS_COMPLETE ||
      progress_info.completed_save_count != 1u) {
    return false;
  }

  persistence_state.completed_save_count = 1u;
  persistence_state.log_used_size = 0u;
  persistence_state.available = true;
  return true;
}

/**
 * @brief
 * 永続化領域を復元する。フラッシュ上のデータを使用してRAM上にキーマップを復元する。
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
static bool _persistence_restore(void) {
  // フラッシュ上のマーカーを読み込む
  uint8_t marker[PWMK_PERSISTENCE_MARKER_SIZE];
  if (!persistence_flash_read(persistence_flash_layout.marker_offset, marker,
                              sizeof(marker))) {
    return false;
  }
  // マーカーが一致しない場合は復元を失敗させる
  if (memcmp(marker, PWMK_PERSISTENCE_MARKER, sizeof(marker)) != 0) {
    return false;
  }

  // フラッシュ上のシリアル番号を読み込む
  uint8_t serial[PWMK_FIRMWARE_SERIAL_SIZE];
  if (!persistence_flash_read(persistence_flash_layout.serial_offset, serial,
                              sizeof(serial))) {
    return false;
  }
  // シリアル番号が一致しない場合は古いデータとみなし、復元を失敗させる
  if (memcmp(serial, pwmk_firmware_serial, sizeof(serial)) != 0) {
    return false;
  }

  // 保存進捗を読み込み、保存完了済みであることを確認する
  pwmk_progress_info_t progress_info;
  if (!_persistence_read_progress(&progress_info) ||
      progress_info.state != PWMK_PROGRESS_COMPLETE ||
      progress_info.completed_save_count == 0u) {
    return false;
  }

  // フラッシュ上の構築済みデータを読み込む
  uint8_t built_data[PWMK_BUILT_DATA_SIZE];
  if (!persistence_flash_read(persistence_flash_layout.built_data_offset,
                              built_data, sizeof(built_data))) {
    return false;
  }

  const uint8_t *read_log_address;
  if (!persistence_flash_get_view(persistence_flash_layout.log_data_offset,
                                  persistence_flash_layout.log_data_size,
                                  &read_log_address)) {
    return false;
  }

  // フラッシュ上の書き込みログを再生して、バッファに反映する
  size_t log_used_size;
  if (!persistence_format_log_replay(
          read_log_address, persistence_flash_layout.log_data_size,
          progress_info.completed_save_count - 1u, built_data,
          sizeof(built_data), &log_used_size)) {
    return false;
  }

  // バッファからキーマップをインポートする
  if (!keymap_import_pwmk(built_data, sizeof(built_data))) {
    return false;
  }

  persistence_state.completed_save_count = progress_info.completed_save_count;
  persistence_state.log_used_size = log_used_size;
  persistence_state.available = true;
  return true;
}

/**
 * @brief ログ領域に保存するための空き容量があるかどうかをチェックする
 * @param record_size 保存するレコードのサイズ
 * @return 空き容量がある場合はtrue、空き容量がない場合はfalse
 */
static bool _persistence_log_has_space(size_t record_size) {

  return persistence_state.log_used_size <=
             persistence_flash_layout.log_data_size &&
         record_size <= persistence_flash_layout.log_data_size -
                            persistence_state.log_used_size;
}

/**
 * @brief 書き込みログをフラッシュに書き込む
 * @param record 書き込むログレコードのバッファ
 * @param record_size 保存するログレコードのサイズ
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
static bool _persistence_append_log(const uint8_t *record, size_t record_size) {
  if (record == NULL) {
    return false;
  }

  // 次の保存進捗スロットを取得する
  pwmk_progress_info_t current_progress = {
      .state = PWMK_PROGRESS_COMPLETE,
      .completed_save_count = persistence_state.completed_save_count,
  };
  size_t slot;
  if (!persistence_format_progress_get_next_slot(&current_progress, &slot)) {
    return false;
  }

  if (!_persistence_log_has_space(record_size)) {
    return false;
  }

  // 保存進捗を保存中にする
  if (!_persistence_update_progress(slot, false)) {
    return false;
  }
  // 書き込みログをフラッシュに書き込む
  if (!persistence_flash_program(persistence_flash_layout.log_data_offset +
                                     persistence_state.log_used_size,
                                 record, record_size)) {
    return false;
  }
  // 保存進捗を保存完了にする
  if (!_persistence_update_progress(slot, true)) {
    return false;
  }

  // 保存進捗を読み込んで、正しく更新されたか確認する
  pwmk_progress_info_t updated_progress;
  if (!_persistence_read_progress(&updated_progress)) {
    return false;
  }
  if (updated_progress.state != PWMK_PROGRESS_COMPLETE ||
      updated_progress.completed_save_count !=
          persistence_state.completed_save_count + 1u) {
    return false;
  }

  persistence_state.completed_save_count =
      updated_progress.completed_save_count;
  persistence_state.log_used_size += record_size;
  return true;
}

/**
 * @brief 永続化領域の管理機能を初期化する
 * @return 成功した場合はtrue、失敗した場合はfalse
 * @note 永続化領域のデータを直接初期化する目的の関数ではない。
 */
bool persistence_init(void) {
  persistence_state = (persistence_state_t){0};

  if (!persistence_flash_init()) {
    return false;
  }

  if (_persistence_restore()) {
    return true;
  }
  return _persistence_rebuild(true);
}

/**
 * @brief キーマップの指定した位置にキーコードを保存する
 * @param layer 保存するレイヤー番号
 * @param row 保存する行番号
 * @param col 保存する列番号
 * @param keycode 保存する内部キーコード
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_set_keycode(uint8_t layer, uint8_t row, uint8_t col,
                             icode_t keycode) {

  // キーマップの指定した位置に保存するための、
  // 構築済みデータの先頭からのオフセットを取得する
  size_t keymap_data_index;
  if (!persistence_state.available || !keymap_is_valid_keycode(keycode) ||
      !keymap_get_pwmk_offset(layer, row, col, &keymap_data_index)) {
    return false;
  }

  // すでに同じキーコードが保存されている場合は、何もせず成功とする
  const icode_t previous_keycode = keymap_get(layer, row, col);
  if (previous_keycode == keycode) {
    return true;
  }

  // 現在のRAM上の構築済みデータを取得する
  uint8_t previous_built_data[PWMK_BUILT_DATA_SIZE];
  if (!keymap_export_pwmk(previous_built_data, sizeof(previous_built_data))) {
    return false;
  }

  // 書き込みログをバッファに作成する
  uint8_t
      record[PWMK_VARIABLE_LOG_HEADER_SIZE + PWMK_VARIABLE_LOG_MAX_DATA_SIZE];
  size_t record_size;
  const uint32_t value = (uint32_t)keycode;
  uint8_t encoded_keycode[PWMK_KEYCODE_SIZE];
  encoded_keycode[0] = (uint8_t)value;
  encoded_keycode[1] = (uint8_t)(value >> 8u);
  encoded_keycode[2] = (uint8_t)(value >> 16u);
  encoded_keycode[3] = (uint8_t)(value >> 24u);
  if (!persistence_format_log_encode(
          record, sizeof(record), &record_size, previous_built_data,
          sizeof(previous_built_data), keymap_data_index, encoded_keycode,
          sizeof(encoded_keycode))) {
    return false;
  }

  // RAM上のキーマップを更新する
  if (!keymap_set(layer, row, col, keycode)) {
    return false;
  }

  bool saved;
  if (_persistence_log_has_space(record_size) &&
      persistence_state.completed_save_count < PWMK_PROGRESS_BIT_COUNT / 2u) {
    // 書き込みログを追加する十分なスペースがある場合は、フラッシュに追記する
    saved = _persistence_append_log(record, record_size);
  } else {
    // スペースがない場合は永続化領域をRAM上のキーマップで初期化して空き容量を確保する
    saved = _persistence_rebuild(false);
  }

  if (saved) {
    return true;
  }

  persistence_state.available = false;
  return false;
}

/**
 * @brief
 * 永続化領域を初期化する。規定のキーマップを使用して永続化領域を初期化する。
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_reset_keymap(void) {
  // 永続化領域を規定のキーマップで初期化する
  if (_persistence_rebuild(true)) {
    return true;
  }

  persistence_state.available = false;
  return false;
}

/**
 * @brief 永続化領域が使用可能かどうかをチェックする
 * @return 使用可能な場合はtrue、使用不可能な場合はfalse
 */
bool persistence_is_available(void) { return persistence_state.available; }
