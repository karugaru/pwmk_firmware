#ifndef PWMK_EVENT_H
#define PWMK_EVENT_H

#include "hid/hid.h"
#include "keyboard/code.h"
#include <stdbool.h>
#include <stdint.h>

// HIDレポートのデータ構造
typedef struct {
  uint8_t report_id;                 // HIDレポートID
  uint16_t size;                     // HIDレポートのサイズ
  uint8_t data[HID_REPORT_SIZE_MAX]; // HIDレポートのデータ
} event_hid_report_t;

typedef bool (*event_platform_cb_t)(icode_t icode, bool pressed);
typedef icode_t (*keymap_get_cb_t)(uint8_t layer, uint8_t row, uint8_t col);

// イベント処理の設定
typedef struct {
  int16_t mouse_move_thresh;  // マウス移動イベントを発生させるための閾値
  int16_t mouse_wheel_thresh; // マウスホイールイベントを発生させるための閾値
  int16_t mouse_move_delta;   // マウス移動イベントの1回あたりの移動量
  int16_t mouse_wheel_delta;  // マウスホイールイベントの1回あたりの移動量
  // プラットフォーム固有のイベント処理コールバック
  event_platform_cb_t platform_callback;
  // キーマップから内部用のキーコードを取得するコールバック
  keymap_get_cb_t keymap_get_callback;
} event_settings_t;

void event_init(event_settings_t settings);

int8_t event_request_pointing_device_id(void);

void event_accumulate_mouse(uint8_t device_id, code_mouse_button_t buttons,
                            int8_t x, int8_t y, int8_t w);
void event_process(uint8_t row, uint8_t col, bool pressed, uint64_t event_time);
void event_process_periodic(void);

bool event_has_event(void);
bool event_pop_hid_report(event_hid_report_t *report);

/**
 * @brief ユーザー定義イベントを処理する。
 * @param icode 内部コード。上書きされた場合、その内容が標準イベント処理に渡される。
 * @param pressed 押されたか離されたか
 * @param event_time イベントの発生時刻
 * @return 標準イベント処理を続行する場合はtrue、しない場合はfalse
 */
bool event_process_user_cb(icode_t *icode, bool pressed, uint64_t event_time);

#endif // PWMK_EVENT_H
