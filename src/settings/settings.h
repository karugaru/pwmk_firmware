#ifndef PWMK_SETTINGS_H
#define PWMK_SETTINGS_H

#include "keyboard/code.h"
#include "profile/settings.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 設定の更新結果を表す列挙型
typedef enum {
  SETTINGS_UPDATE_FAILED,    // 失敗
  SETTINGS_UPDATE_PERSISTED, // 成功
  SETTINGS_UPDATE_RAM_ONLY,  // 永続化は失敗したが、RAMへの反映は成功した
} settings_update_result_t;

// 設定の識別子を表す列挙型
typedef enum {
  SETTINGS_ID_KEYMAP, // キーマップ設定
} settings_id_t;

void settings_init(void);
size_t settings_image_size(void);
icode_t settings_get_keycode(uint8_t layer, uint8_t row, uint8_t col);
settings_update_result_t settings_set_keycode(uint8_t layer, uint8_t row,
                                              uint8_t col, icode_t keycode);
settings_update_result_t settings_reset(settings_id_t id);

#endif // PWMK_SETTINGS_H
