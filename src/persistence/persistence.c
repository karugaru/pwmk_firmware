#include "persistence/persistence.h"
#include "persistence/persistence_flash.h"
#include "persistence/persistence_format.h"
#include "profile/persistence_identity.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef DEBUG_PERSISTENCE
#define DEBUG_PERSISTENCE 0
#endif

#if DEBUG_PERSISTENCE
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)(0))
#endif

// 永続化の状態を表す構造体
typedef struct {
  bool available;              // 永続化が有効かどうか
  size_t image_size;           // 永続化イメージのサイズ
  size_t completed_save_count; // 完了済みの保存回数
  size_t log_used_size;        // 使用済みのログ領域サイズ
} persistence_state_t;

static persistence_state_t persistence_state;

/*
 * 内部関数宣言
 */

static bool _persistence_image_is_valid(const uint8_t *image,
                                        size_t image_size);
static bool _persistence_update_progress(size_t slot, bool mark_completed);
static bool _persistence_read_progress(pwmk_progress_info_t *info);
static bool _persistence_can_append(size_t record_size);
static bool _persistence_append_log(const uint8_t *record, size_t record_size);

/*
 * 公開関数
 */

/**
 * @brief 永続化領域の管理機能を初期化する
 * @param image_size 永続化イメージのサイズ
 * @return 初期化に成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_init(size_t image_size) {
  persistence_state = (persistence_state_t){.image_size = image_size};
  const bool result = persistence_flash_init(image_size);
  DEBUG_PRINT("Persistence init: image_size=%zu result=%d\n", image_size,
              result);
  return result;
}

/**
 * @brief 永続化領域からイメージを復元する
 * @param image 復元先のバッファへのポインタ
 * @param image_size 復元先のバッファのサイズ
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_restore(uint8_t *image, size_t image_size) {
  DEBUG_PRINT("Persistence restore: image_size=%zu\n", image_size);
  if (!_persistence_image_is_valid(image, image_size)) {
    DEBUG_PRINT("Persistence restore failed: invalid image\n");
    return false;
  }

  // フラッシュ上のマーカーを読み込む
  uint8_t marker[PWMK_PERSISTENCE_MARKER_SIZE];
  if (!persistence_flash_read(persistence_flash_layout.marker_offset, marker,
                              sizeof(marker))) {
    DEBUG_PRINT("Persistence restore failed: marker read\n");
    return false;
  }
  // マーカーが一致しない場合は復元を失敗させる
  if (memcmp(marker, PWMK_PERSISTENCE_MARKER, sizeof(marker)) != 0) {
    DEBUG_PRINT("Persistence restore failed: marker mismatch\n");
    return false;
  }

  // フラッシュ上のシリアル番号を読み込む
  uint8_t serial[PWMK_FIRMWARE_SERIAL_SIZE];
  if (!persistence_flash_read(persistence_flash_layout.serial_offset, serial,
                              sizeof(serial))) {
    DEBUG_PRINT("Persistence restore failed: serial read\n");
    return false;
  }
  // シリアル番号が一致しない場合は古いデータとみなし、復元を失敗させる
  if (memcmp(serial, pwmk_firmware_serial, sizeof(serial)) != 0) {
    DEBUG_PRINT("Persistence restore failed: serial mismatch\n");
    return false;
  }

  // 保存進捗を読み込み、保存完了済みであることを確認する
  pwmk_progress_info_t progress_info;
  if (!_persistence_read_progress(&progress_info)) {
    DEBUG_PRINT("Persistence restore failed: progress read\n");
    return false;
  }
  if (progress_info.state != PWMK_PROGRESS_COMPLETE ||
      progress_info.completed_save_count == 0u) {
    DEBUG_PRINT("Persistence restore failed: progress state=%d saves=%zu\n",
                progress_info.state, progress_info.completed_save_count);
    return false;
  }

  // フラッシュ上の構築済みデータを読み込む
  if (!persistence_flash_read(persistence_flash_layout.built_data_offset, image,
                              image_size)) {
    DEBUG_PRINT("Persistence restore failed: built data read\n");
    return false;
  }

  const uint8_t *read_log_address;
  if (!persistence_flash_get_view(persistence_flash_layout.log_data_offset,
                                  persistence_flash_layout.log_data_size,
                                  &read_log_address)) {
    DEBUG_PRINT("Persistence restore failed: log view\n");
    return false;
  }

  // フラッシュ上の書き込みログを再生して、バッファに反映する
  size_t log_used_size;
  if (!persistence_format_log_replay(read_log_address,
                                     persistence_flash_layout.log_data_size,
                                     progress_info.completed_save_count - 1u,
                                     image, image_size, &log_used_size)) {
    DEBUG_PRINT("Persistence restore failed: log replay\n");
    return false;
  }

  persistence_state.completed_save_count = progress_info.completed_save_count;
  persistence_state.log_used_size = log_used_size;
  persistence_state.available = true;
  DEBUG_PRINT("Persistence restore complete: saves=%zu log_used=%zu\n",
              persistence_state.completed_save_count,
              persistence_state.log_used_size);
  return true;
}

/**
 * @brief 永続化領域にイメージを保存する
 * @param image 保存するイメージへのポインタ
 * @param image_size 保存するイメージのサイズ
 * @return 成功した場合はtrue、失敗した場合はfalse
 * @note
 * この関数は保存のたびに
 * 1. フラッシュのデータをすべて読み込む
 * 2. 読み込んだデータからイメージを再構築する
 * 3. 保存するデータとの差分をフル比較する
 * 4. 差分から書き込みログを生成する
 * といったナイーブな実装となっている。
 * そのため将来的には、1・2を永続化領域のデータのキャッシュに置き換える、
 * 3をイメージ内のdirtyな領域のみを比較するなどの最適化が必要になる可能性がある。
 */
bool persistence_commit(const uint8_t *image, size_t image_size) {
  DEBUG_PRINT("Persistence commit: image_size=%zu\n", image_size);
  // 永続化可能かどうかをチェックする
  if (!_persistence_image_is_valid(image, image_size)) {
    DEBUG_PRINT("Persistence commit failed: invalid image\n");
    return false;
  }
  if (!persistence_state.available) {
    DEBUG_PRINT("Persistence commit failed: persistence unavailable\n");
    return false;
  }

  // フラッシュ上のデータを読み込む
  uint8_t saved_image[persistence_state.image_size];
  if (!persistence_restore(saved_image, image_size)) {
    DEBUG_PRINT("Persistence commit failed: restore current image\n");
    persistence_state.available = false;
    return false;
  }

  // 保存するイメージと保存済みのイメージを比較し、差分を計算する
  size_t first_difference = image_size;
  size_t last_difference = 0u;
  for (size_t index = 0; index < image_size; index++) {
    if (saved_image[index] != image[index]) {
      if (first_difference == image_size) {
        first_difference = index;
      }
      last_difference = index;
    }
  }

  if (first_difference == image_size) {
    DEBUG_PRINT("Persistence commit: unchanged\n");
    return true;
  }

  // 差分がある場合は、差分を書き込みログとして保存する
  size_t offset = first_difference;
  size_t remaining = last_difference - first_difference + 1u;
  DEBUG_PRINT("Persistence commit diff: offset=%zu size=%zu\n", offset,
              remaining);
  while (remaining > 0u) {
    const size_t data_size = remaining < PWMK_VARIABLE_LOG_MAX_DATA_SIZE
                                 ? remaining
                                 : PWMK_VARIABLE_LOG_MAX_DATA_SIZE;
    uint8_t
        record[PWMK_VARIABLE_LOG_HEADER_SIZE + PWMK_VARIABLE_LOG_MAX_DATA_SIZE];
    size_t record_size;
    if (!persistence_format_log_encode(record, sizeof(record), &record_size,
                                       saved_image, image_size, offset,
                                       &image[offset], data_size)) {
      DEBUG_PRINT("Persistence commit failed: log encode offset=%zu size=%zu\n",
                  offset, data_size);
      persistence_state.available = false;
      return false;
    }

    if (!_persistence_can_append(record_size)) {
      DEBUG_PRINT("Persistence commit: append unavailable, rebuilding "
                  "record_size=%zu "
                  "used=%zu capacity=%zu\n",
                  record_size, persistence_state.log_used_size,
                  persistence_flash_layout.log_data_size);
      const bool result = persistence_rebuild(image, image_size);
      DEBUG_PRINT("Persistence commit rebuild result=%d\n", result);
      return result;
    }
    if (!_persistence_append_log(record, record_size)) {
      DEBUG_PRINT("Persistence commit failed: log append\n");
      persistence_state.available = false;
      return false;
    }

    offset += data_size;
    remaining -= data_size;
  }

  DEBUG_PRINT("Persistence commit complete: saves=%zu log_used=%zu\n",
              persistence_state.completed_save_count,
              persistence_state.log_used_size);
  return true;
}

/**
 * @brief 永続化領域を指定したイメージから再構築する
 * @param image 永続化イメージへのポインタ
 * @param image_size 永続化イメージのサイズ
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_rebuild(const uint8_t *image, size_t image_size) {
  DEBUG_PRINT("Persistence rebuild: image_size=%zu\n", image_size);
  persistence_state.available = false;
  if (!_persistence_image_is_valid(image, image_size)) {
    DEBUG_PRINT("Persistence rebuild failed: invalid image\n");
    return false;
  }

  // フラッシュ領域を消去する
  if (!persistence_flash_erase_all()) {
    DEBUG_PRINT("Persistence rebuild failed: flash erase\n");
    return false;
  }
  // マーカーを書き込む
  if (!persistence_flash_program(persistence_flash_layout.marker_offset,
                                 (const uint8_t *)PWMK_PERSISTENCE_MARKER,
                                 PWMK_PERSISTENCE_MARKER_SIZE)) {
    DEBUG_PRINT("Persistence rebuild failed: marker write\n");
    return false;
  }
  // シリアル番号を書き込む
  if (!persistence_flash_program(persistence_flash_layout.serial_offset,
                                 pwmk_firmware_serial,
                                 PWMK_FIRMWARE_SERIAL_SIZE)) {
    DEBUG_PRINT("Persistence rebuild failed: serial write\n");
    return false;
  }
  // 保存進捗を保存中にする
  if (!_persistence_update_progress(0u, false)) {
    DEBUG_PRINT("Persistence rebuild failed: progress start\n");
    return false;
  }
  // 構築済みデータを書き込む
  if (!persistence_flash_program(persistence_flash_layout.built_data_offset,
                                 image, image_size)) {
    DEBUG_PRINT("Persistence rebuild failed: built data write\n");
    return false;
  }
  // 保存進捗を保存完了にする
  if (!_persistence_update_progress(0u, true)) {
    DEBUG_PRINT("Persistence rebuild failed: progress complete\n");
    return false;
  }
  // 保存進捗を読み込んで、正しく初期化されたか確認する
  pwmk_progress_info_t progress_info;
  if (!_persistence_read_progress(&progress_info)) {
    DEBUG_PRINT("Persistence rebuild failed: progress verify read\n");
    return false;
  }
  if (progress_info.state != PWMK_PROGRESS_COMPLETE ||
      progress_info.completed_save_count != 1u) {
    DEBUG_PRINT("Persistence rebuild failed: progress verify state=%d "
                "saves=%zu\n",
                progress_info.state, progress_info.completed_save_count);
    return false;
  }

  persistence_state.completed_save_count = 1u;
  persistence_state.log_used_size = 0u;
  persistence_state.available = true;
  DEBUG_PRINT("Persistence rebuild complete\n");
  return true;
}

/*
 * 内部関数
 */

/**
 * @brief 永続化イメージが有効かどうかを判定する
 * @param image 永続化イメージへのポインタ
 * @param image_size 永続化イメージのサイズ
 * @return 有効なイメージの場合はtrue、無効なイメージの場合はfalse
 */
static bool _persistence_image_is_valid(const uint8_t *image,
                                        size_t image_size) {
  return image != NULL && image_size == persistence_state.image_size &&
         image_size > 0u;
}

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
    DEBUG_PRINT("Persistence progress update failed: slot=%zu completed=%d\n",
                slot, mark_completed);
    return false;
  }

  // 現在のスロットの値をフラッシュから読み込む
  const size_t offset = persistence_flash_layout.progress_offset + byte_offset;
  uint8_t current_value;
  if (!persistence_flash_read(offset, &current_value, sizeof(current_value))) {
    DEBUG_PRINT("Persistence progress update failed: read offset=%zu\n",
                offset);
    return false;
  }

  // 更新用の値を計算する
  uint8_t updated_value;
  if (!persistence_format_progress_update_slot(
          current_value, slot, mark_completed, &updated_value)) {
    DEBUG_PRINT("Persistence progress update failed: invalid value slot=%zu "
                "current=0x%02X\n",
                slot, current_value);
    return false;
  }

  // フラッシュに更新後の値を書き込む
  const bool result =
      persistence_flash_program(offset, &updated_value, sizeof(updated_value));
  DEBUG_PRINT("Persistence progress update: slot=%zu completed=%d "
              "value=0x%02X->0x%02X result=%d\n",
              slot, mark_completed, current_value, updated_value, result);
  return result;
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
    DEBUG_PRINT("Persistence progress read failed\n");
    return false;
  }

  // 読み込んだ保存進捗を解析する
  const bool result =
      persistence_format_progress_analyze(progress, sizeof(progress), info);
  if (result) {
    DEBUG_PRINT("Persistence progress: state=%d saves=%zu\n", info->state,
                info->completed_save_count);
  } else {
    DEBUG_PRINT("Persistence progress parse failed\n");
  }
  return result;
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
 * @brief フラッシュに書き込みログを追記できるかどうかを判定する
 * @param record_size 追記するレコードのサイズ
 * @return 追記可能な場合はtrue、追記不可能な場合はfalse
 */
static bool _persistence_can_append(size_t record_size) {
  pwmk_progress_info_t current_progress = {
      .state = PWMK_PROGRESS_COMPLETE,
      .completed_save_count = persistence_state.completed_save_count,
  };
  size_t slot;
  return persistence_format_progress_get_next_slot(&current_progress, &slot) &&
         _persistence_log_has_space(record_size);
}

/**
 * @brief 書き込みログをフラッシュに書き込む
 * @param record 書き込むログレコードのバッファ
 * @param record_size 保存するログレコードのサイズ
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
static bool _persistence_append_log(const uint8_t *record, size_t record_size) {
  if (record == NULL || !_persistence_can_append(record_size)) {
    DEBUG_PRINT("Persistence log append failed: record_size=%zu\n",
                record_size);
    return false;
  }

  // 次の保存進捗スロットを取得する
  pwmk_progress_info_t current_progress = {
      .state = PWMK_PROGRESS_COMPLETE,
      .completed_save_count = persistence_state.completed_save_count,
  };
  size_t slot;
  if (!persistence_format_progress_get_next_slot(&current_progress, &slot)) {
    DEBUG_PRINT("Persistence log append failed: no progress slot\n");
    return false;
  }

  DEBUG_PRINT("Persistence log append: slot=%zu offset=%zu size=%zu\n", slot,
              persistence_flash_layout.log_data_offset +
                  persistence_state.log_used_size,
              record_size);

  // 保存進捗を保存中にする
  if (!_persistence_update_progress(slot, false)) {
    DEBUG_PRINT("Persistence log append failed: progress start\n");
    return false;
  }
  // 書き込みログをフラッシュに書き込む
  if (!persistence_flash_program(persistence_flash_layout.log_data_offset +
                                     persistence_state.log_used_size,
                                 record, record_size)) {
    DEBUG_PRINT("Persistence log append failed: log write\n");
    return false;
  }
  // 保存進捗を保存完了にする
  if (!_persistence_update_progress(slot, true)) {
    DEBUG_PRINT("Persistence log append failed: progress complete\n");
    return false;
  }

  // 保存進捗を読み込んで、正しく更新されたか確認する
  pwmk_progress_info_t updated_progress;
  if (!_persistence_read_progress(&updated_progress)) {
    DEBUG_PRINT("Persistence log append failed: progress verify read\n");
    return false;
  }
  if (updated_progress.state != PWMK_PROGRESS_COMPLETE ||
      updated_progress.completed_save_count !=
          persistence_state.completed_save_count + 1u) {
    DEBUG_PRINT("Persistence log append failed: progress verify state=%d "
                "saves=%zu\n",
                updated_progress.state, updated_progress.completed_save_count);
    return false;
  }

  persistence_state.completed_save_count =
      updated_progress.completed_save_count;
  persistence_state.log_used_size += record_size;
  DEBUG_PRINT("Persistence log append complete: saves=%zu log_used=%zu\n",
              persistence_state.completed_save_count,
              persistence_state.log_used_size);
  return true;
}
