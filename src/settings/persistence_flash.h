#ifndef PWMK_PERSISTENCE_FLASH_H
#define PWMK_PERSISTENCE_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// フラッシュ領域上の永続化データのレイアウトを表す構造体
typedef struct {
  size_t serial_offset;     // シリアル番号のオフセット
  size_t progress_offset;   // 保存進捗のオフセット
  size_t built_data_offset; // ビルド済みデータのオフセット
  size_t log_data_offset;   // ログデータのオフセット
  size_t log_data_size;     // ログデータのサイズ
} persistence_flash_layout_t;

extern const persistence_flash_layout_t persistence_flash_layout;

bool persistence_flash_init(void);
bool persistence_flash_read(size_t offset, uint8_t *buffer, size_t size);
bool persistence_flash_get_view(size_t offset, size_t size,
                                const uint8_t **read_address_out);
bool persistence_flash_region_is_erased(size_t offset, size_t size);
bool persistence_flash_program(size_t offset, const uint8_t *data, size_t size);
bool persistence_flash_erase_all(void);

#endif // PWMK_PERSISTENCE_FLASH_H
