#ifndef PWMK_HID_H
#define PWMK_HID_H

#include "keyboard/code.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define HID_KEYBOARD_REPORT_ID 0x01
#define HID_MOUSE_REPORT_ID 0x02
#define HID_CONSUMER_REPORT_ID 0x03
#define HID_REPORT_ID_MAX HID_CONSUMER_REPORT_ID

#define HID_KEYBOARD_REPORT_SIZE 8
#define HID_MOUSE_REPORT_SIZE 4
#define HID_CONSUMER_REPORT_SIZE 12
#define HID_REPORT_SIZE_MAX                                                    \
  (MAX(MAX(HID_CONSUMER_REPORT_SIZE, HID_KEYBOARD_REPORT_SIZE),                \
       HID_MOUSE_REPORT_SIZE))

#define HID_POINTING_DEVICE_MAX 2

extern const uint8_t hid_descriptor[];
extern const uint8_t hid_descriptor_len;

// HID内部状態
typedef struct {
  bool has_keyboard_event; // キーボードイベントが発生したかどうか
  // キーボードの状態
  struct {
    code_mod_bits_t real_modifier;    // 実際に押されている修飾子
    code_mod_bits_t virtual_modifier; // 仮想的に押されている修飾子
    code_t keycode[6];                // 実際に押されているキーコード
  } keyboard;

  bool has_mouse_event; // マウスイベントが発生したかどうか
  // マウスの状態を保持する配列
  struct {
    code_mouse_button_t buttons; // マウスボタンの状態
    int16_t xDelta;              // X軸の移動量
    int16_t yDelta;              // Y軸の移動量
    int16_t wDelta;              // スクロールホイールの移動量
  } mouse[HID_POINTING_DEVICE_MAX];
  int8_t pointing_id_max; // 現在使用中のマウスデバイスIDの最大値

  bool has_consumer_event; // コンシューマーイベントが発生したかどうか
  // コンシューマーの状態
  struct {
    code_consumer_t keycode[6]; // 実際に押されているコンシューマーキーコード
  } consumer;
} hid_state_t;

void hid_keyboard_to_report(hid_state_t *event,
                            uint8_t report[HID_KEYBOARD_REPORT_SIZE]);
void hid_mouse_to_report_and_consume(hid_state_t *event,
                                     uint8_t report[HID_MOUSE_REPORT_SIZE],
                                     int16_t mouse_move_thresh,
                                     int16_t mouse_wheel_thresh);
void hid_consumer_to_report(hid_state_t *event,
                            uint8_t report[HID_CONSUMER_REPORT_SIZE]);

int8_t hid_request_pointing_device_id(hid_state_t *state);

#endif // PWMK_HID_H
