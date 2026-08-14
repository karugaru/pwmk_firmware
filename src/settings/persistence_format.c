#include "settings/persistence_format.h"
#include <string.h>

/**
 * @brief 配列がすべて0xFFで埋まっているかを確認する
 * @param data 確認する配列
 * @param size 配列のサイズ
 * @return すべて0xFFで埋まっている場合はtrue、それ以外の場合はfalse
 */
static bool all_ff(const uint8_t *data, size_t size) {
  for (size_t index = 0; index < size; index++) {
    if (data[index] != 0xFFu) {
      return false;
    }
  }
  return true;
}

/**
 * @brief 保存進捗の状態を解析する
 * @param progress 解析する保存進捗の配列
 * @param progress_size 配列のサイズ
 * @param info 解析結果を格納する構造体へのポインタ
 */
bool persistence_format_progress_analyze(const uint8_t *progress,
                                         size_t progress_size,
                                         pwmk_progress_info_t *info) {
  if (progress == NULL || info == NULL || progress_size != PWMK_PROGRESS_SIZE) {
    return false;
  }

  size_t completed_save_count = 0;
  bool unused_slot_seen = false;
  bool in_progress_slot_seen = false;

  // 進捗保存配列の全てのスロット(2ビット単位)を解析する
  // 0b00: 完了済み、0b01: 保存中、0b11: 未使用、0b10: 破損
  for (size_t slot = 0; slot < PWMK_PROGRESS_BIT_COUNT / 2u; slot++) {
    const size_t byte_offset = slot / 4u;
    const uint8_t shift_amount = (uint8_t)(6u - 2u * (slot % 4u));
    const uint8_t pair = (progress[byte_offset] >> shift_amount) & 0x03u;

    // 保存中のスロットが見つかったにもかかわらず、その後で未使用ではないスロットが見つかった場合は破損とみなす
    if (in_progress_slot_seen) {
      if (pair != 0b11u) {
        info->state = PWMK_PROGRESS_DIRTY;
        info->completed_save_count = completed_save_count;
        return true;
      }
      continue;
    }

    // 未使用のスロットが見つかったにもかかわらず、その後で未使用ではないスロットが見つかった場合は破損とみなす
    if (unused_slot_seen) {
      if (pair != 0b11u) {
        info->state = PWMK_PROGRESS_DIRTY;
        info->completed_save_count = completed_save_count;
        return true;
      }
      continue;
    }

    switch (pair) {
    case 0b00u:
      completed_save_count++;
      break;
    case 0b01u:
      in_progress_slot_seen = true;
      break;
    case 0b11u:
      unused_slot_seen = true;
      break;
    default:
      info->state = PWMK_PROGRESS_DIRTY;
      info->completed_save_count = completed_save_count;
      return true;
    }
  }

  info->completed_save_count = completed_save_count;
  if (in_progress_slot_seen) {
    info->state = PWMK_PROGRESS_IN_PROGRESS;
  } else if (completed_save_count == 0u) {
    info->state = PWMK_PROGRESS_EMPTY;
  } else {
    info->state = PWMK_PROGRESS_COMPLETE;
  }
  return true;
}

/**
 * @brief 次の保存スロットを取得する
 * @param info 解析済みの保存進捗情報へのポインタ
 * @param slot 次の保存スロットを格納する変数へのポインタ
 * @return 次の保存スロットが取得できた場合はtrue、それ以外の場合はfalse
 */
bool persistence_format_progress_get_next_slot(const pwmk_progress_info_t *info,
                                               size_t *slot) {
  if (info == NULL || slot == NULL || info->state != PWMK_PROGRESS_COMPLETE ||
      info->completed_save_count >= PWMK_PROGRESS_BIT_COUNT / 2u) {
    return false;
  }

  // 保存回数は次の保存スロットの番号と一致する。
  *slot = info->completed_save_count;
  return true;
}

/**
 * @brief 保存スロットの更新に必要な情報を取得する
 * @param slot 更新する保存スロットの番号
 * @param mark_completed
 * スロットを保存済とする場合はtrue、保存中とする場合はfalse
 * @param byte_offset 更新するバイトのオフセットを格納する変数へのポインタ
 * @param clear_mask 更新するバイトのクリアマスクを格納する変数へのポインタ
 * @return 情報が取得できた場合はtrue、それ以外の場合はfalse
 */
bool persistence_format_progress_get_slot_update(size_t slot,
                                                 bool mark_completed,
                                                 size_t *byte_offset,
                                                 uint8_t *clear_mask) {
  if (byte_offset == NULL || clear_mask == NULL ||
      slot >= PWMK_PROGRESS_BIT_COUNT / 2u) {
    return false;
  }

  const uint8_t shift_amount = (uint8_t)(6u - 2u * (slot % 4u));
  *byte_offset = slot / 4u;
  *clear_mask = (uint8_t)(1u << (shift_amount + (mark_completed ? 0u : 1u)));
  return true;
}

/**
 * @brief 保存スロットの値を更新する
 * @param current_value 現在の保存スロットの値
 * @param slot 更新する保存スロットの番号
 * @param mark_completed
 * スロットを保存済とする場合はtrue、保存中とする場合はfalse
 * @param updated_value 更新後の保存スロットの値を格納する変数へのポインタ
 * @return 更新が成功した場合はtrue、それ以外の場合はfalse
 */
bool persistence_format_progress_update_slot(uint8_t current_value, size_t slot,
                                             bool mark_completed,
                                             uint8_t *updated_value) {
  size_t byte_offset;
  uint8_t clear_mask;
  if (updated_value == NULL ||
      !persistence_format_progress_get_slot_update(slot, mark_completed,
                                                   &byte_offset, &clear_mask) ||
      (current_value & clear_mask) == 0u) {
    return false;
  }

  // スロットを保存済にする場合、保存中(0b01)のスロットからのみ遷移を許可する。
  // 上位ビットが1の場合は未使用(0b11)のため、保存済にはできない。
  if (mark_completed && (current_value & (uint8_t)(clear_mask << 1u)) != 0u) {
    return false;
  }

  // スロットを保存中にする場合、未使用(0b11)のスロットからのみ遷移を許可する。
  // 下位ビットが0の場合は破損(0b10)のため、保存中にはできない。
  if (!mark_completed && (current_value & (clear_mask >> 1u)) == 0u) {
    return false;
  }

  *updated_value = (uint8_t)(current_value & ~clear_mask);
  return true;
}

/**
 * @brief 書き込みログをエンコードする
 * @param record エンコード後の書き込みログが書き込まれる配列
 * @param record_size 配列のサイズ
 * @param encoded_size
 * エンコード後の書き込みログのサイズが書き込まれる変数のポインタ
 * @param built_data
 * 再生時にこの書き込みログが反映される予定の配列
 * @param built_data_size 配列のサイズ
 * @param relative_offset
 * 書き込むデータの先頭位置(built_dataの先頭からの相対オフセット)
 * @param data 書き込むデータが格納されている配列
 * @param data_size 配列のサイズ
 */
bool persistence_format_log_encode(uint8_t *record, size_t record_size,
                                   size_t *encoded_size,
                                   const uint8_t *built_data,
                                   size_t built_data_size,
                                   size_t relative_offset, const uint8_t *data,
                                   size_t data_size) {
  if (record == NULL || encoded_size == NULL || built_data == NULL ||
      data == NULL || data_size == 0u ||
      data_size > PWMK_VARIABLE_LOG_MAX_DATA_SIZE ||
      relative_offset > UINT16_MAX || relative_offset > built_data_size ||
      data_size > built_data_size - relative_offset) {
    return false;
  }

  // 固定長書き込みログを使用する条件は、
  // データサイズが固定長以下であること
  // かつ相対オフセットが0xFFXXではないこと
  // かつ書き込み予定先のサイズが固定長以上であること
  // かつ書き込んだデータが書き込み予定先の範囲内に収まること。
  const bool use_fixed =
      data_size <= PWMK_FIXED_LOG_DATA_SIZE &&
      (relative_offset >> 8u) != PWMK_VARIABLE_LOG_TYPE &&
      built_data_size >= PWMK_FIXED_LOG_DATA_SIZE &&
      relative_offset <= built_data_size - PWMK_FIXED_LOG_DATA_SIZE;

  // 固定長書き込みログを使用する場合
  if (use_fixed) {
    // この関数の結果を格納できない場合は失敗とする
    if (record_size < PWMK_FIXED_LOG_RECORD_SIZE) {
      return false;
    }

    // エンコードする。
    // 固定長書き込みでは書き込むデータが固定長より小さい場合に備えて
    // built_dataの内容を先にコピーしておき、dataの内容で上書きようにデータを用意する。
    record[0] = (uint8_t)(relative_offset >> 8u);
    record[1] = (uint8_t)relative_offset;
    memcpy(&record[2], &built_data[relative_offset], PWMK_FIXED_LOG_DATA_SIZE);
    memcpy(&record[2], data, data_size);
    *encoded_size = PWMK_FIXED_LOG_RECORD_SIZE;
    return true;
  }

  // 可変長書き込みログを使用する場合

  const size_t variable_size = PWMK_VARIABLE_LOG_HEADER_SIZE + data_size;

  // この関数の結果を格納できない場合は失敗とする
  if (record_size < variable_size) {
    return false;
  }

  // エンコードする。
  record[0] = PWMK_VARIABLE_LOG_TYPE;
  record[1] = (uint8_t)data_size;
  record[2] = (uint8_t)(relative_offset >> 8u);
  record[3] = (uint8_t)relative_offset;
  memcpy(&record[PWMK_VARIABLE_LOG_HEADER_SIZE], data, data_size);
  *encoded_size = variable_size;
  return true;
}

/**
 * @brief 書き込みログを再生する
 * @param log 再生する書き込みログが複数連続して格納されている配列
 * @param log_size 配列のサイズ
 * @param expected_record_count 再生されるログの予定数
 * @param built_data ログを反映する先の配列
 * @param built_data_size 配列のサイズ
 * @param used_size 再生したログの合計サイズが書き込まれる変数へのポインタ
 * @return 再生が成功した場合はtrue、それ以外の場合はfalse
 */
bool persistence_format_log_replay(const uint8_t *log, size_t log_size,
                                   size_t expected_record_count,
                                   uint8_t *built_data, size_t built_data_size,
                                   size_t *used_size) {
  if (log == NULL || built_data == NULL) {
    return false;
  }

  // log配列内の現在の位置を示す
  size_t position = 0;

  // 予定されているログの数だけ再生する
  for (size_t record_count = 0; record_count < expected_record_count;
       record_count++) {

    // 現在の位置がlog配列のサイズを超えている場合は無効とみなす
    // この場合、書き込みログの構成自体が破損していることになる。
    if (position >= log_size) {
      return false;
    }

    const uint8_t first = log[position];

    // 固定長ログの場合
    if (first != PWMK_VARIABLE_LOG_TYPE) {
      // ログの長さが規定の長さに満たない場合は無効とみなす
      if (log_size - position < PWMK_FIXED_LOG_RECORD_SIZE) {
        return false;
      }

      // 書き込むデータの相対オフセットを計算する。
      const size_t relative_offset = ((size_t)first << 8u) | log[position + 1u];

      // 書き込むデータの範囲がbuilt_dataの範囲を超える場合は無効とみなす
      if (relative_offset > built_data_size ||
          PWMK_FIXED_LOG_DATA_SIZE > built_data_size - relative_offset) {
        return false;
      }

      // データをbuilt_dataに書き込む。
      memcpy(&built_data[relative_offset], &log[position + 2u],
             PWMK_FIXED_LOG_DATA_SIZE);
      position += PWMK_FIXED_LOG_RECORD_SIZE;
      continue;
    }

    // 可変長ログの場合

    // ログのヘッダが規定の長さに満たない場合、またはヘッダがすべて初期値(0xFF)で埋まっている場合は無効とみなす
    if (log_size - position < PWMK_VARIABLE_LOG_HEADER_SIZE ||
        all_ff(&log[position], PWMK_VARIABLE_LOG_HEADER_SIZE)) {
      return false;
    }

    // データサイズと相対オフセットを取得する
    const size_t data_size = log[position + 1u];
    const size_t relative_offset =
        ((size_t)log[position + 2u] << 8u) | log[position + 3u];

    // データサイズが0の場合、またはbuilt_dataの範囲を超える場合は無効とみなす
    if (data_size == 0u ||
        log_size - position < PWMK_VARIABLE_LOG_HEADER_SIZE + data_size ||
        relative_offset > built_data_size ||
        data_size > built_data_size - relative_offset) {
      return false;
    }

    // データをbuilt_dataに書き込む。
    memcpy(&built_data[relative_offset],
           &log[position + PWMK_VARIABLE_LOG_HEADER_SIZE], data_size);
    position += PWMK_VARIABLE_LOG_HEADER_SIZE + data_size;
  }

  // 予定されているログの数だけ再生した後、残りのログがすべて初期値(0xFF)で埋まっていることを確認する
  if (!all_ff(&log[position], log_size - position)) {
    return false;
  }

  // 再生したログの合計サイズはpositionと一致する
  if (used_size != NULL) {
    *used_size = position;
  }

  return true;
}
