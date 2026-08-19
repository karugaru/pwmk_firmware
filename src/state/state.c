#include <hardware/watchdog.h>
#include <pico/bootrom.h>

#include "ble/ble.h"
#include "led/led.h"
#include "state/sleep.h"
#include "state/state.h"
#include "usb/usb_hid.h"

// LEDの状態を定義する構造体
typedef struct {
  uint8_t r, g, b; // LEDのRGB値
} state_led_entry_t;

// clang-format off
static const state_led_entry_t state_led_table[] = {
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

static volatile state_system_t current_state = STATE_RESET;
static volatile state_conn_pref_t conn_pref = CONN_PREF_USB;

/*
 * 内部関数宣言
 */

static bool _state_is_runtime_state(state_system_t state);
static state_system_t _state_resolve_runtime(void);

/*
 * 公開関数
 */

/**
 * @brief システムの優先接続モードを設定する。
 * @param pref 設定する優先接続モード
 */
void state_set_connection_preference(state_conn_pref_t pref) {
  conn_pref = pref;
  state_refresh_runtime();
}

/**
 * @brief システムの優先接続モードを取得する。
 * @return 現在の優先接続モード
 */
state_conn_pref_t state_get_connection_preference(void) { return conn_pref; }

/**
 * @brief 優先接続モードを切り替える。
 * @param pref 切り替える優先接続モード
 */
void state_switch_connection_preference(state_conn_pref_t pref) {
  conn_pref = pref;
  // BLE優先に切り替えた場合、BLEが有効でなければ有効にする
  if (pref == CONN_PREF_BLE) {
    if (!ble_is_enabled()) {
      ble_power_set(true);
    }
  }

  state_refresh_runtime();
}

/**
 * @brief 現在の接続状況と優先接続モードから状態を更新する。
 */
void state_refresh_runtime(void) {
  if (!_state_is_runtime_state(current_state) &&
      current_state != STATE_INIT_COMPLETE) {
    return;
  }

  state_set_system(_state_resolve_runtime());
}

/**
 * @brief システム状態を設定する。
 * @param new_state 設定する新しい状態
 */
void state_set_system(state_system_t new_state) {
  const state_led_entry_t *entry = &state_led_table[new_state];

  if (current_state == new_state) {
    return;
  }

  led_put_rgb(entry->r, entry->g, entry->b);
  current_state = new_state;

  switch (new_state) {
  case STATE_RESET:
    watchdog_reboot(0, 0, 0);
    watchdog_enable(0, 1);
    return;
  case STATE_BOOTLOADER:
    reset_usb_boot(0, 0);
    return;
  case STATE_DEEP_SLEEP:
    sleep_enter_deep();
    return;
  default:
    return;
  }
}

/**
 * @brief システムの現在の状態を取得する。
 * @return 現在のシステム状態
 */
state_system_t state_get_system(void) { return current_state; }

/*
 * 内部関数
 */

/**
 * @brief 指定された状態がランタイム状態かどうかを返す。
 *        ランタイム状態とは、接続待機中や接続中など、通常運用中のことを指す。
 * @param state 判定する状態
 * @return stateがランタイム状態の場合はtrue、それ以外はfalse
 */
static bool _state_is_runtime_state(state_system_t state) {
  return state == STATE_USB_WAITING || state == STATE_BLE_WAITING ||
         state == STATE_BLE_CONNECTED || state == STATE_USB_CONNECTED;
}

/**
 * @brief 現在の接続状況と優先接続モードから状態を解決する。
 * @return 解決された状態
 */
static state_system_t _state_resolve_runtime(void) {
  bool usb_active = usb_hid_is_active();
  bool ble_connected = ble_is_connected();

  if (usb_active) {
    return STATE_USB_CONNECTED;
  } else if (ble_connected) {
    return STATE_BLE_CONNECTED;
  }

#if PWMK_ENABLE_USB && PWMK_ENABLE_BLE
  if (conn_pref == CONN_PREF_BLE) {
    return STATE_BLE_WAITING;
  } else if (conn_pref == CONN_PREF_USB) {
    return STATE_USB_WAITING;
  }
#elif PWMK_ENABLE_BLE
  return STATE_BLE_WAITING;
#elif PWMK_ENABLE_USB
  return STATE_USB_WAITING;
#endif

  return STATE_INIT_COMPLETE;
}
