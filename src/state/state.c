#include "state/state.h"
#include "ble/ble.h"
#include "state/sleep.h"
#include "usb/usb_hid.h"

// 現在の定常状態
static volatile state_steady_t current_steady_state = STATE_BOOTING;
// 現在の一時状態
static volatile state_temporary_t current_temporary_state = STATE_TEMP_NONE;
// 要求された定常状態
static volatile state_steady_t requested_steady_state = STATE_NONE;

// 優先する接続モード
static volatile state_conn_pref_t conn_pref = CONN_PREF_USB;

/*
 * 内部関数宣言
 */

static bool _state_is_runtime_state(state_steady_t state);
static state_steady_t _state_resolve_runtime(void);
static void _state_transition(state_steady_t state);

/*
 * 公開関数
 */

/**
 * @brief システムの優先接続モードを設定する。
 * @param pref 設定する優先接続モード
 */
void state_set_connection_preference(state_conn_pref_t pref) {
  conn_pref = pref;
}

/**
 * @brief システムの優先接続モードを取得する。
 * @return 現在の優先接続モード
 */
state_conn_pref_t state_get_connection_preference(void) { return conn_pref; }

/**
 * @brief 定常状態への移行を要求する。
 * @param state 移行先の定常状態
 */
void state_request_steady_state(state_steady_t state) {
  requested_steady_state = state;
}

/**
 * @brief 現在の定常状態を取得する。
 * @return 現在の定常状態
 */
state_steady_t state_get_steady_state(void) { return current_steady_state; }

/**
 * @brief 一時状態を設定する。
 * @param state 設定する一時状態
 */
void state_set_temporary_state(state_temporary_t state) {
  current_temporary_state = state;
}

/**
 * @brief 現在の一時状態を取得する。
 * @return 現在の一時状態
 */
state_temporary_t state_get_temporary_state(void) {
  return current_temporary_state;
}

/**
 * @brief 現在の一時状態をクリアする。
 */
void state_clear_temporary_state(void) {
  current_temporary_state = STATE_TEMP_NONE;
}

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
}

/**
 * @brief 定期的な状態管理処理を実行する。
 */
void state_process_periodic(void) {
  // 要求された定常状態があれば遷移する
  if (requested_steady_state != STATE_NONE) {
    _state_transition(requested_steady_state);
    requested_steady_state = STATE_NONE;
    return;
  }

  // ランタイム状態または初期化完了状態の場合は、接続状況に応じて状態を解決する
  if (_state_is_runtime_state(current_steady_state) ||
      current_steady_state == STATE_INIT_COMPLETE) {
    _state_transition(_state_resolve_runtime());
  }
}

/*
 * 内部関数
 */

/**
 * @brief 定常状態を遷移させる。
 * @param state 遷移先の定常状態
 */
static void _state_transition(state_steady_t state) {
  if (current_steady_state == state) {
    return;
  }

  current_steady_state = state;

  switch (state) {
  case STATE_DEEP_SLEEP:
    sleep_enter_deep();
    return;
  default:
    return;
  }
}

/**
 * @brief 指定された状態がランタイム状態かどうかを返す。
 *        ランタイム状態とは、接続待機中や接続中など、通常運用中のことを指す。
 * @param state 判定する状態
 * @return stateがランタイム状態の場合はtrue、それ以外はfalse
 */
static bool _state_is_runtime_state(state_steady_t state) {
  return state == STATE_USB_WAITING || state == STATE_BLE_WAITING ||
         state == STATE_BLE_CONNECTED || state == STATE_USB_CONNECTED;
}

/**
 * @brief 現在の接続状況と優先接続モードから状態を解決する。
 * @return 解決された状態
 */
static state_steady_t _state_resolve_runtime(void) {
  bool usb_active = usb_hid_is_active();
  bool ble_connected = ble_is_connected();

  if (conn_pref == CONN_PREF_BLE) {
    if (ble_connected) {
      return STATE_BLE_CONNECTED;
    } else if (usb_active) {
      return STATE_USB_CONNECTED;
    }
  } else {
    if (usb_active) {
      return STATE_USB_CONNECTED;
    } else if (ble_connected) {
      return STATE_BLE_CONNECTED;
    }
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
