#ifndef PWMK_PERSISTENCE_FORMAT_H
#define PWMK_PERSISTENCE_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ファームウェアのシリアル番号のサイズ
#define PWMK_FIRMWARE_SERIAL_SIZE 16u
// 永続化領域を識別するマーカー
#define PWMK_PERSISTENCE_MARKER "PWMK"
// 永続化領域を識別するマーカーのサイズ
#define PWMK_PERSISTENCE_MARKER_SIZE (sizeof(PWMK_PERSISTENCE_MARKER) - 1u)
// ヘッダの予約分サイズ
#define PWMK_HEADER_RESERVED_SIZE 236u
// ヘッダのサイズ
#define PWMK_HEADER_SIZE                                                       \
  (PWMK_PERSISTENCE_MARKER_SIZE + PWMK_FIRMWARE_SERIAL_SIZE +                  \
   PWMK_HEADER_RESERVED_SIZE)
// 進捗保存バイトサイズ
#define PWMK_PROGRESS_SIZE 512u
// 進捗保存バイトサイズのビット換算
#define PWMK_PROGRESS_BIT_COUNT (PWMK_PROGRESS_SIZE * 8u)
// 固定長書き込みログのデータ部のサイズ
#define PWMK_FIXED_LOG_DATA_SIZE 4u
// 固定長書き込みログのレコード全長サイズ
#define PWMK_FIXED_LOG_RECORD_SIZE (2u + PWMK_FIXED_LOG_DATA_SIZE)
// 可変長書き込みログのヘッダ部のサイズ
#define PWMK_VARIABLE_LOG_HEADER_SIZE 4u
// 可変長書き込みログのデータ部の最大サイズ
#define PWMK_VARIABLE_LOG_MAX_DATA_SIZE 255u
// 可変長書き込みログのタイプ値
#define PWMK_VARIABLE_LOG_TYPE 0xFFu

// 保存進捗を表す列挙型。
// 注意点として、これは保存進捗スロットの値を表すものではなく、
// 保存進捗全体の状態の表現である。
typedef enum {
  PWMK_PROGRESS_EMPTY,       // 保存が一度も行われていない状態
  PWMK_PROGRESS_COMPLETE,    // 保存完了
  PWMK_PROGRESS_IN_PROGRESS, // 保存中
  PWMK_PROGRESS_DIRTY,       // 破損
} pwmk_progress_state_t;

// 保存進捗情報を表す構造体
typedef struct {
  pwmk_progress_state_t state; // 保存進捗の状態
  size_t completed_save_count; // 完了済みの保存回数
} pwmk_progress_info_t;

bool persistence_format_progress_analyze(const uint8_t *progress,
                                         size_t progress_size,
                                         pwmk_progress_info_t *info);
bool persistence_format_progress_get_next_slot(const pwmk_progress_info_t *info,
                                               size_t *slot);
bool persistence_format_progress_get_slot_update(size_t slot,
                                                 bool mark_completed,
                                                 size_t *byte_offset,
                                                 uint8_t *clear_mask);
bool persistence_format_progress_update_slot(uint8_t current_value, size_t slot,
                                             bool mark_completed,
                                             uint8_t *updated_value);

bool persistence_format_log_encode(uint8_t *record, size_t record_size,
                                   size_t *encoded_size,
                                   const uint8_t *built_data,
                                   size_t built_data_size,
                                   size_t relative_offset, const uint8_t *data,
                                   size_t data_size);
bool persistence_format_log_replay(const uint8_t *log, size_t log_size,
                                   size_t expected_record_count,
                                   uint8_t *built_data, size_t built_data_size,
                                   size_t *used_size);

#endif // PWMK_PERSISTENCE_FORMAT_H
