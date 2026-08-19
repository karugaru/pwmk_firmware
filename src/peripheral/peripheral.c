#include "peripheral/peripheral.h"
#include "hid/hid.h"
#include "keyboard/event.h"
#include "led/led.h"
#include "pinnacle/pinnacle.h"
#include "profile/board.h"
#include "settings/settings.h"

// LEDの状態を定義する構造体
typedef struct {
  uint8_t r, g, b; // LEDのRGB値
} peripheral_led_entry_t;

// clang-format off
static const peripheral_led_entry_t peripheral_led_table[] = {
  [STATE_RESET]         = { 0,   0,   0   }, // 消灯
  [STATE_BOOTING]       = { 255, 127, 0   }, // オレンジ
  [STATE_SYS_INIT]      = { 255, 255, 0   }, // 黄色
  [STATE_BLE_INIT]      = { 0,   0,   255 }, // 青
  [STATE_INIT_COMPLETE] = { 255, 255, 255 }, // 白
  [STATE_USB_WAITING]   = { 255, 0,   0   }, // 赤
  [STATE_BLE_WAITING]   = { 0,   255, 255 }, // 水色
  [STATE_BLE_CONNECTED] = { 0,   0,   0   }, // 消灯
  [STATE_USB_CONNECTED] = { 0,   0,   0   }, // 消灯
  [STATE_BOOTLOADER]    = { 255, 255, 255 }, // 白
  [STATE_DEEP_SLEEP]    = { 0,   0,   0   }, // 消灯
};
// clang-format on

#ifndef DEBUG_PERIPHERAL
#define DEBUG_PERIPHERAL 0
#endif

#if DEBUG_PERIPHERAL
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)(0))
#endif

static int8_t pointing_device_pinnacle = -1;

/*
 * 公開関数
 */

/**
 * @brief 通常のペリフェラルより前に初期化が必要なペリフェラルを初期化する。
 */
void peripheral_early_init(void) { led_init(GPIO_LED_PIN, LED_BRIGHTNESS); }

/**
 * @brief 周期的なペリフェラル処理を行う。
 */
void peripheral_process_periodic(void) {
  state_system_t state = state_get_system();
  const peripheral_led_entry_t *entry = &peripheral_led_table[state];
  led_put_rgb(entry->r, entry->g, entry->b);
}

/**
 * @brief 周辺機器の初期化を行う。
 * @return bool 初期化に成功した場合にtrueを返す。
 */
bool peripheral_init(void) {
#if USE_PINNACLE
  // I2Cとトラックパッドの初期化
  if (!pinnacle_init(i2c0, GPIO_SCL_PIN, GPIO_SDA_PIN, GPIO_DR_PIN)) {
    DEBUG_PRINT("failed to initialise pinnacle\n");
    return false;
  }
  pinnacle_set_speed(PINNACLE_ACCEL, PINNACLE_SPEED);
  pinnacle_set_rotation(PINNACLE_ROTATE);
  pointing_device_pinnacle = event_request_pointing_device_id();
  if (pointing_device_pinnacle < 0) {
    DEBUG_PRINT("failed to request pointing device id for pinnacle\n");
    return false;
  }
#endif

  return true;
}

/**
 * @brief 周辺機器のイベント処理が必要かどうかを取得する。
 * @return bool イベント処理が必要な場合にtrueを返す。
 */
bool peripheral_require_event_processing(void) {
  // トラックパッドのDRピンがアクティブならBLEにイベント処理を行うよう要求する
  if (USE_PINNACLE && pinnacle_check_DR()) {
    return true;
  }

  return false;
}

/**
 * @brief 周辺機器のイベント処理を行う。
 */
void peripheral_process_events(void) {
  if (USE_PINNACLE) {
    pinnacle_data_t tdata;
    while (pinnacle_read_data(&tdata)) {
      if (pointing_device_pinnacle >= 0) {
        event_accumulate_mouse(pointing_device_pinnacle, tdata.buttons,
                               tdata.xDelta, tdata.yDelta, tdata.wDelta);
      }
    }
  }
}
