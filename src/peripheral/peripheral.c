#include "peripheral/peripheral.h"
#include "debug.h"
#include "hid/hid.h"
#include "keyboard/event.h"
#include "led/led.h"
#include "pinnacle/pinnacle.h"
#include "profile/board.h"
#include "settings/settings.h"
#include <pico/bootrom.h>
#include <pico/stdlib.h>

// LEDの1回点滅するサイクルにかかる時間（ミリ秒）
#define BLINK_CYCLE_MS 500
// LEDの各点滅シーケンスの間の無点灯時間（ミリ秒）
#define BLINK_PAUSE_MS 1000
// LEDの1回点滅するサイクル中の点灯時間（ミリ秒）
#define BLINK_DURATION_MS 250

// LEDの表示パターンを定義する列挙型
typedef enum {
  PERIPHERAL_LED_OFF,          // 消灯
  PERIPHERAL_LED_SOLID,        // 点灯
  PERIPHERAL_LED_BREATHE_SLOW, // ゆっくり点滅
  PERIPHERAL_LED_BREATHE_FAST, // 速く点滅
  PERIPHERAL_LED_BLINK_COUNT,  // 点滅回数指定
} peripheral_led_pattern_t;

// LEDの表示パターンを表す構造体
typedef struct {
  uint8_t r, g, b;                  // 色を表すRGB値
  peripheral_led_pattern_t pattern; // 表示パターン
  uint8_t count;                    // 点滅回数
} peripheral_led_entry_t;

// clang-format off
// システム状態に応じたLEDの表示パターンを定義するテーブル
static const peripheral_led_entry_t peripheral_led_table[STATE_STEADY_COUNT] = {
  [STATE_BOOTING]               = {255, 127, 0, PERIPHERAL_LED_SOLID, 0},         // オレンジ点灯
  [STATE_SYS_INIT]              = {255, 255, 0, PERIPHERAL_LED_SOLID, 0},         // 黄色点灯
  [STATE_BLE_INIT]              = {0, 255, 255, PERIPHERAL_LED_SOLID, 0},         // 水色点灯
  [STATE_INIT_COMPLETE]         = {255, 255, 255, PERIPHERAL_LED_SOLID, 0},       // 白色点灯
  [STATE_USB_WAITING]           = {0, 0, 255, PERIPHERAL_LED_BREATHE_SLOW, 0},    // 青色ゆっくり点滅
  [STATE_BLE_WAITING]           = {0, 255, 255, PERIPHERAL_LED_BREATHE_SLOW, 0},  // 水色ゆっくり点滅
  [STATE_BLE_CONNECTED]         = {0, 255, 255, PERIPHERAL_LED_OFF, 0},           // 水色消灯
  [STATE_USB_CONNECTED]         = {0, 0, 255, PERIPHERAL_LED_OFF, 0},             // 青色消灯
  [STATE_BOOTLOADER]            = {255, 255, 255, PERIPHERAL_LED_SOLID, 0},       // 白色点灯
  [STATE_DEEP_SLEEP]            = {0, 0, 0, PERIPHERAL_LED_OFF, 0},               // 消灯
  [STATE_INIT_ERROR_PERIPHERAL] = {255, 0, 0, PERIPHERAL_LED_BLINK_COUNT, 1},     // 赤色1回点滅
  [STATE_INIT_ERROR_BLE]        = {255, 0, 0, PERIPHERAL_LED_BLINK_COUNT, 2},     // 赤色2回点滅
  [STATE_INIT_ERROR_SETTINGS]   = {255, 0, 0, PERIPHERAL_LED_BLINK_COUNT, 3},     // 赤色3回点滅
};
// clang-format on

#ifndef DEBUG_PERIPHERAL
#define DEBUG_PERIPHERAL 0
#endif

#if DEBUG_PERIPHERAL
#define DEBUG_PRINT(...) pwmk_debug_printf("PERIPHERAL", __VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)(0))
#endif

// トラックパッドのデバイスID
static int8_t pointing_device_pinnacle = -1;
// 現在表示している一時状態
static state_temporary_t rendered_temporary_state = STATE_TEMP_NONE;
// 一時状態が開始された時刻（ミリ秒）
static uint32_t temporary_state_started_at_ms;

/*
 * 内部関数宣言
 */

static peripheral_led_entry_t _temporary_entry(state_temporary_t state);
static uint32_t _temporary_duration_ms(state_temporary_t state);
static void _render_entry(peripheral_led_entry_t entry, uint32_t elapsed_ms);

/*
 * 公開関数
 */

/**
 * @brief 通常のペリフェラルより前に初期化が必要なペリフェラルを初期化する。
 */
void peripheral_early_init(void) { led_init(GPIO_LED_PIN, LED_BRIGHTNESS); }

/**
 * @brief ディープスリープに入る前のペリフェラルの準備を行う。
 */
void peripheral_prepare_deep_sleep(void) {
  led_put_rgb(0, 0, 0);
  sleep_ms(1);
}

/**
 * @brief 周期的なペリフェラル処理を行う。
 */
void peripheral_process_periodic(void) {
  uint32_t now_ms = to_ms_since_boot(get_absolute_time());
  state_steady_t steady_state = state_get_steady_state();

  // ブートローダーモードの場合は即座にブートローダーに遷移する
  if (steady_state == STATE_BOOTLOADER) {
    _render_entry(peripheral_led_table[STATE_BOOTLOADER], now_ms);
    sleep_ms(1);
    reset_usb_boot(0, 0);
    return;
  }

  // 表示中の一時状態と現在の一時状態が異なる場合は更新する
  state_temporary_t temporary_state = state_get_temporary_state();
  if (temporary_state != rendered_temporary_state) {
    rendered_temporary_state = temporary_state;
    temporary_state_started_at_ms = now_ms;
  }

  // 一時状態の表示時間が経過した場合は一時状態を解除する
  uint32_t duration = _temporary_duration_ms(temporary_state);
  if (now_ms - temporary_state_started_at_ms >= duration) {
    temporary_state = STATE_TEMP_NONE;
    state_clear_temporary_state();
  }

  // 一時状態がある場合は表示する
  if (temporary_state != STATE_TEMP_NONE) {
    _render_entry(_temporary_entry(temporary_state),
                  now_ms - temporary_state_started_at_ms);
    return;
  }

  // 一時状態がない場合は定常状態に応じた表示を行う
  peripheral_led_entry_t entry = peripheral_led_table[steady_state];
  _render_entry(entry, now_ms);
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

/*
 * 内部関数
 */

/**
 * @brief 指定された一時状態に対応するLED表示パターンを取得する。
 * @param state 一時状態
 * @return LED表示パターン
 */
static peripheral_led_entry_t _temporary_entry(state_temporary_t state) {
  peripheral_led_entry_t entry = {0, 0, 0, PERIPHERAL_LED_OFF, 0};

  switch (state) {
  case STATE_TEMP_SETTINGS_SAVING:
  case STATE_TEMP_VIAL_UNLOCKING:
    entry =
        (peripheral_led_entry_t){255, 255, 0, PERIPHERAL_LED_BREATHE_FAST, 0};
    break;
  case STATE_TEMP_SETTINGS_PERSISTED:
  case STATE_TEMP_VIAL_UNLOCKED:
    entry = (peripheral_led_entry_t){0, 255, 0, PERIPHERAL_LED_BLINK_COUNT, 1};
    break;
  case STATE_TEMP_SETTINGS_RAM_ONLY:
    entry =
        (peripheral_led_entry_t){255, 127, 0, PERIPHERAL_LED_BLINK_COUNT, 2};
    break;
  case STATE_TEMP_SETTINGS_FAILED:
    entry = (peripheral_led_entry_t){255, 0, 0, PERIPHERAL_LED_BLINK_COUNT, 3};
    break;
  case STATE_TEMP_VIAL_LOCKED:
    entry = (peripheral_led_entry_t){255, 0, 0, PERIPHERAL_LED_BLINK_COUNT, 1};
    break;
  case STATE_TEMP_CONNECTION_SWITCHED:
    if (state_get_connection_preference() == CONN_PREF_BLE) {
      entry =
          (peripheral_led_entry_t){0, 255, 255, PERIPHERAL_LED_BLINK_COUNT, 1};
    } else {
      entry =
          (peripheral_led_entry_t){0, 0, 255, PERIPHERAL_LED_BLINK_COUNT, 1};
    }
    break;
  case STATE_TEMP_BLE_SLOT_CHANGED:
    entry =
        (peripheral_led_entry_t){0, 255, 255, PERIPHERAL_LED_BLINK_COUNT, 1};
    break;
  case STATE_TEMP_NONE:
    break;
  }

  return entry;
}

/**
 * @brief 指定された一時状態の規定の表示時間を取得する。
 * @param state 一時状態
 * @return 表示時間（ミリ秒）
 */
static uint32_t _temporary_duration_ms(state_temporary_t state) {
  switch (state) {
  case STATE_TEMP_SETTINGS_SAVING:
    return 1200;
  case STATE_TEMP_SETTINGS_PERSISTED:
  case STATE_TEMP_SETTINGS_RAM_ONLY:
  case STATE_TEMP_SETTINGS_FAILED:
    return 1100;
  case STATE_TEMP_VIAL_UNLOCKED:
  case STATE_TEMP_VIAL_LOCKED:
  case STATE_TEMP_CONNECTION_SWITCHED:
  case STATE_TEMP_BLE_SLOT_CHANGED:
  case STATE_TEMP_VIAL_UNLOCKING:
    return 500;
  case STATE_TEMP_NONE:
    return 0;
  }

  return 0;
}

/**
 * @brief 三角波を計算する。
 * @param elapsed_ms 経過時間（ミリ秒）
 * @param period_ms 周期（ミリ秒）
 * @return 波高（0〜255）
 */
static uint8_t _triangle_level(uint32_t elapsed_ms, uint32_t period_ms) {
  uint32_t phase = elapsed_ms % period_ms;
  uint32_t half_period = period_ms / 2;
  if (phase < half_period) {
    return (uint8_t)((phase * 255) / half_period);
  }
  return (uint8_t)(((period_ms - phase) * 255) / half_period);
}

/**
 * @brief LEDを表示パターンに従って制御する。
 * @param entry LED表示パターン
 * @param elapsed_ms 経過時間（ミリ秒）
 */
static void _render_entry(peripheral_led_entry_t entry, uint32_t elapsed_ms) {
  uint8_t level = 255;

  switch (entry.pattern) {
  case PERIPHERAL_LED_OFF:
    level = 0;
    break;
  case PERIPHERAL_LED_SOLID:
    break;
  case PERIPHERAL_LED_BREATHE_SLOW:
    level = _triangle_level(elapsed_ms, 2000);
    break;
  case PERIPHERAL_LED_BREATHE_FAST:
    level = _triangle_level(elapsed_ms, 600);
    break;
  case PERIPHERAL_LED_BLINK_COUNT: {
    const uint8_t blink_count = entry.count == 0 ? 1 : entry.count;

    const uint32_t total = blink_count * BLINK_CYCLE_MS;
    const uint32_t in_seq = elapsed_ms % (total + BLINK_PAUSE_MS);
    const uint32_t in_cycle = in_seq % BLINK_CYCLE_MS;
    const bool is_blinking = in_seq < total && in_cycle < BLINK_DURATION_MS;

    level = is_blinking ? 255 : 0;
    break;
  }
  }

  led_put_rgb((uint8_t)(((uint16_t)entry.r * level) / 255),
              (uint8_t)(((uint16_t)entry.g * level) / 255),
              (uint8_t)(((uint16_t)entry.b * level) / 255));
}
