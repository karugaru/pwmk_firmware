#include "persistence/persistence_flash.h"
#include "persistence/persistence_format.h"
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <pico/binary_info.h>
#include <pico/btstack_flash_bank.h>
#include <pico/flash.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @note 注意事項
 *
 * RP2040およびRP2350では、フラッシュの読み込みと書き込みとでアドレスの指定方法が異なる。
 * 読み込みの場合はXIP_BASE系を基準として指定する必要があり、
 * アクセスはポインタを介しての変数参照で行う。
 * 書き込みの場合は0を基準として指定する必要があり、
 * アクセスはflash_range_program()やflash_range_erase()を介して行う。
 * 例えば物理フラッシュの先頭から0x1000バイト目のアドレスを指定する場合、
 * 読み込みではXIP_BASE + 0x1000、書き込みでは0x1000となる。
 *
 * このファイルでは、アドレスには次の3つの表現があり、
 * 可能な限りoffsetのみを使用するようにする。
 *
 * `read_address`は`フラッシュの先頭を基準にした実際に読み込みに行くアドレスで、
 * 値は常にPWMK_FLASH_READ_BASE + PWMK_PERSISTENCE_START以上である。
 * マクロのREAD_ADDRESS(offset)を使用して取得。
 *
 * `write_address`は0を基準にしたAPI呼び出しに使用するアドレスで、
 * 値は常にPWMK_PERSISTENCE_START以上である。
 * マクロのWRITE_ADDRESS(offset)を使用して取得する。
 *
 * `offset`はPWMK_PERSISTENCE_STARTを基準にした相対オフセットで、
 * 値は常に0以上である。単体ではフラッシュのアドレスを表すことはできない。
 */

// アドレスを指定のアライメントに揃えるマクロ
#define PWMK_ALIGN_UP(value, alignment)                                        \
  (((value) + (alignment) - 1u) / (alignment) * (alignment))

// 永続化に使用するフラッシュ領域のセクタ数
#define PWMK_PERSISTENCE_SECTOR_COUNT 4u
// 永続化に使用するフラッシュ領域のサイズ
#define PWMK_PERSISTENCE_SIZE                                                  \
  (PWMK_PERSISTENCE_SECTOR_COUNT * FLASH_SECTOR_SIZE)
// 永続化に使用するフラッシュ領域が始まるアドレス。(write_address形式)
// btstackが使用する領域の直前に重ならないように配置している。
#define PWMK_PERSISTENCE_START                                                 \
  (PICO_FLASH_BANK_STORAGE_OFFSET - PWMK_PERSISTENCE_SIZE)

// 永続化領域を識別するマーカーのオフセット
#define PWMK_MARKER_OFFSET 0u
// シリアル番号のオフセット。
#define PWMK_SERIAL_OFFSET (PWMK_MARKER_OFFSET + PWMK_PERSISTENCE_MARKER_SIZE)
// 読み込みに使用するフラッシュの基準アドレス
#if PICO_RP2040
#define PWMK_FLASH_READ_BASE XIP_NOCACHE_NOALLOC_BASE
#else
#define PWMK_FLASH_READ_BASE XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE
#endif

// 読み込みに使用するフラッシュのアドレスを取得するマクロ
#define READ_ADDRESS(offset)                                                   \
  (PWMK_FLASH_READ_BASE + PWMK_PERSISTENCE_START + (offset))
// 読み込みに使用するフラッシュのアドレスを取得するマクロ（ポインタ型）
#define READ_ADDRESS_PTR(offset)                                               \
  ((const uint8_t *)(uintptr_t)(READ_ADDRESS(offset)))
// 書き込みに使用するフラッシュのアドレスを取得するマクロ
#define WRITE_ADDRESS(offset) (PWMK_PERSISTENCE_START + (offset))

// 永続化領域のサイズ計算に誤りがある
_Static_assert(PWMK_PERSISTENCE_SIZE % FLASH_SECTOR_SIZE == 0u,
               "PWMK persistence size must be sector aligned");
// 永続化領域の開始位置の計算に誤りがある
_Static_assert(PWMK_PERSISTENCE_START % FLASH_SECTOR_SIZE == 0u,
               "PWMK persistence start must be sector aligned");
// シリアル番号がヘッダ内に収まらない
_Static_assert(PWMK_SERIAL_OFFSET + PWMK_FIRMWARE_SERIAL_SIZE <=
                   PWMK_HEADER_SIZE,
               "PWMK serial does not fit in header");
// 書き込みログ1つの大きさがページ境界をまたぐ
_Static_assert(PWMK_FIXED_LOG_RECORD_SIZE <= FLASH_PAGE_SIZE,
               "PWMK fixed log record exceeds a flash page");
// 永続化領域のサイズが大きすぎる
_Static_assert(PICO_FLASH_BANK_STORAGE_OFFSET >= PWMK_PERSISTENCE_SIZE,
               "PWMK persistence offset is before flash start");
// 永続化領域のサイズが大きすぎる
_Static_assert(PWMK_PERSISTENCE_START + PWMK_PERSISTENCE_SIZE <=
                   PICO_FLASH_SIZE_BYTES,
               "PWMK persistence extends beyond flash");

bi_decl(bi_block_device(BINARY_INFO_MAKE_TAG('P', 'W'), "PWMK firmware", 0u,
                        PWMK_PERSISTENCE_START, NULL,
                        BINARY_INFO_BLOCK_DEV_FLAG_READ));

persistence_flash_layout_t persistence_flash_layout;
static bool persistence_flash_ready;

// フラッシュ書き込み操作のパラメータを表す構造体
typedef struct {
  bool erase;          // trueの場合は消去、falseの場合は書き込み
  uint32_t offset;     // フラッシュの相対オフセット
  const uint8_t *data; // 書き込むデータのポインタ（消去の場合はNULL）
  size_t size; // 書き込むデータのサイズ（消去の場合は消去する範囲のサイズ）
} persistence_write_param_t;

/*
 * 内部関数宣言
 */

static void __not_in_flash_func(_persistence_flash_operation)(void *parameter);

static bool _persistence_flash_range_is_valid(size_t offset, size_t size);

/*
 * 公開関数
 */

/**
 * @brief フラッシュの初期化を行う
 * @param image_size 保存イメージのサイズ
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_flash_init(size_t image_size) {
  // フラッシュのレイアウトを計算する

  // 不変のレイアウト部分を初期化する
  persistence_flash_layout_t layout = {
      .marker_offset = PWMK_MARKER_OFFSET,
      .serial_offset = PWMK_SERIAL_OFFSET,
      .progress_offset = PWMK_ALIGN_UP(PWMK_HEADER_SIZE, FLASH_PAGE_SIZE),
  };

  // 構築済みデータのオフセットを計算する
  const size_t built_data_offset = PWMK_ALIGN_UP(
      layout.progress_offset + PWMK_PROGRESS_SIZE, FLASH_PAGE_SIZE);
  if (image_size == 0u || built_data_offset > PWMK_PERSISTENCE_SIZE ||
      image_size > PWMK_PERSISTENCE_SIZE - built_data_offset) {
    persistence_flash_ready = false;
    return false;
  }
  layout.built_data_offset = built_data_offset;

  // ログデータのオフセットとサイズを計算する
  layout.log_data_offset =
      PWMK_ALIGN_UP(built_data_offset + image_size, FLASH_PAGE_SIZE);
  if (layout.log_data_offset > PWMK_PERSISTENCE_SIZE) {
    persistence_flash_ready = false;
    return false;
  }
  layout.log_data_size = PWMK_PERSISTENCE_SIZE - layout.log_data_offset;
  if (layout.log_data_size < PWMK_FIXED_LOG_RECORD_SIZE ||
      layout.log_data_size < PWMK_VARIABLE_LOG_HEADER_SIZE + 1u) {
    persistence_flash_ready = false;
    return false;
  }

  persistence_flash_layout = layout;
  persistence_flash_ready = true;
  return true;
}

/**
 * @brief フラッシュからデータを読み込む
 * @param offset フラッシュの相対オフセット
 * @param buffer 読み込んだデータを格納するバッファ
 * @param size 読み込むデータのサイズ
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_flash_read(size_t offset, uint8_t *buffer, size_t size) {
  if (buffer == NULL || !_persistence_flash_range_is_valid(offset, size)) {
    return false;
  }

  memcpy(buffer, READ_ADDRESS_PTR(offset), size);
  return true;
}

/**
 * @brief フラッシュの指定した範囲のデータへのポインタを取得する
 * @param offset フラッシュの相対オフセット
 * @param size 読み込む予定のサイズ
 * @param read_address_out 読み込みアドレスを格納する変数へのポインタ
 * @return 成功した場合はtrue、失敗した場合はfalse
 * @note
 * 読み込みアドレスはポインタや配列としてアクセスすることで
 * そのままフラッシュのデータを読み込むことができる。
 */
bool persistence_flash_get_view(size_t offset, size_t size,
                                const uint8_t **read_address_out) {
  if (read_address_out == NULL ||
      !_persistence_flash_range_is_valid(offset, size)) {
    return false;
  }

  *read_address_out = READ_ADDRESS_PTR(offset);
  return true;
}

/**
 * @brief フラッシュの指定した範囲が消去済みかどうかをチェックする
 * @param offset フラッシュの相対オフセット
 * @param size フラッシュのサイズ
 * @return 消去済みの場合はtrue、消去されていない場合はfalse
 */
bool persistence_flash_region_is_erased(size_t offset, size_t size) {
  if (!_persistence_flash_range_is_valid(offset, size)) {
    return false;
  }

  const uint8_t *data = READ_ADDRESS_PTR(offset);
  for (size_t index = 0; index < size; index++) {
    if (data[index] != 0xFFu) {
      return false;
    }
  }
  return true;
}

/**
 * @brief フラッシュにデータを書き込む
 * @param offset フラッシュの相対オフセット
 * @param data 書き込むデータへのポインタ
 * @param size 書き込むデータのサイズ
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_flash_program(size_t offset, const uint8_t *data,
                               size_t size) {
  if (data == NULL || !_persistence_flash_range_is_valid(offset, size)) {
    return false;
  }

  size_t current_offset = offset;
  size_t remaining = size;

  // 書き込みはページ単位で行う必要があるため、ページごとに分割して書き込む
  while (remaining > 0u) {
    // 現在のオフセットが属するページの先頭アドレスを計算する
    const size_t page_head_offset =
        current_offset / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
    // 現在のオフセットはページの先頭から見るとどの位置にあるかを計算する
    const size_t distance_from_page_head = current_offset - page_head_offset;
    // ページの残り容量を計算する
    const size_t page_remaining = FLASH_PAGE_SIZE - distance_from_page_head;
    // 今見ているページに対して書き込むサイズを計算する
    const size_t write_size =
        remaining < page_remaining ? remaining : page_remaining;

    uint8_t page_buffer[FLASH_PAGE_SIZE];
    persistence_write_param_t write_param = {
        .erase = false,
        .offset = page_head_offset,
        .data = page_buffer,
        .size = FLASH_PAGE_SIZE,
    };

    const uint8_t *page_read_address = READ_ADDRESS_PTR(page_head_offset);

    // ページをバッファに読み込み、書き込みたい範囲だけバッファを更新する
    memcpy(page_buffer, page_read_address, sizeof(page_buffer));
    memcpy(&page_buffer[distance_from_page_head],
           &data[current_offset - offset], write_size);

    // フラッシュの書き込みは1を0にすることしかできないため、
    // 0を1にするような操作が含まれていないかをチェックする
    for (size_t index = distance_from_page_head;
         index < distance_from_page_head + write_size; index++) {
      if ((page_buffer[index] & page_read_address[index]) !=
          page_buffer[index]) {
        return false;
      }
    }

    // フラッシュに書き込み、内容を検証する
    if (flash_safe_execute(_persistence_flash_operation, &write_param,
                           UINT32_MAX) != PICO_OK) {
      return false;
    }
    if (memcmp(READ_ADDRESS_PTR(page_head_offset), page_buffer,
               sizeof(page_buffer)) != 0) {
      return false;
    }

    // 次のページに進める
    current_offset += write_size;
    remaining -= write_size;
  }

  return true;
}

/**
 * @brief フラッシュの全領域を消去する
 * @return 成功した場合はtrue、失敗した場合はfalse
 */
bool persistence_flash_erase_all(void) {
  if (!persistence_flash_ready) {
    return false;
  }

  persistence_write_param_t write_param = {
      .erase = true,
      .offset = 0u,
      .data = NULL,
      .size = PWMK_PERSISTENCE_SIZE,
  };

  // フラッシュを消去し、内容を検証する
  if (flash_safe_execute(_persistence_flash_operation, &write_param,
                         UINT32_MAX) != PICO_OK) {
    return false;
  }
  if (!persistence_flash_region_is_erased(0u, PWMK_PERSISTENCE_SIZE)) {
    return false;
  }

  return true;
}

/*
 * 内部関数
 */

/**
 * @brief フラッシュ書き込み操作を行うコールバック関数
 * @param parameter persistence_write_param_t構造体へのポインタ
 * @details
 * RP2040およびRP2350では、フラッシュの書き込み操作はいくつかの制約があるため、
 * flash_safe_execute()を経由して安全に実行する必要がある。
 * この関数はflash_safe_execute()のコールバック関数として使用される。
 */
static void __not_in_flash_func(_persistence_flash_operation)(void *parameter) {
  // 実際のフラッシュ書き込み操作を行う
  const persistence_write_param_t *write_param = parameter;
  if (write_param->erase) {
    flash_range_erase(WRITE_ADDRESS(write_param->offset), write_param->size);
  } else {
    flash_range_program(WRITE_ADDRESS(write_param->offset), write_param->data,
                        write_param->size);
  }
}

/**
 * @brief 指定した範囲が有効なフラッシュの領域かどうかをチェックする
 * @param offset フラッシュの相対オフセット
 * @param size フラッシュのサイズ
 * @return 有効な範囲の場合はtrue、無効な範囲の場合はfalse
 */
static bool _persistence_flash_range_is_valid(size_t offset, size_t size) {
  return persistence_flash_ready && offset <= PWMK_PERSISTENCE_SIZE &&
         size <= PWMK_PERSISTENCE_SIZE - offset;
}
