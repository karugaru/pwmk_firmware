#ifndef PWMK_STATE_H
#define PWMK_STATE_H

// PWMKのシステム状態を表す列挙型
typedef enum {
  STATE_RESET,         // システムリセット済
  STATE_BOOTING,       // ブート中
  STATE_SYS_INIT,      // システム初期化済
  STATE_BLE_INIT,      // BLE初期化済
  STATE_INIT_COMPLETE, // 全初期化完了
  STATE_USB_WAITING,   // USB接続待機中
  STATE_BLE_WAITING,   // BLE接続待機中
  STATE_BLE_CONNECTED, // BLE接続済
  STATE_USB_CONNECTED, // USB接続済
  STATE_BOOTLOADER,    // ブートローダーモード
  STATE_DEEP_SLEEP,    // ディープスリープモード
} state_system_t;

// PWMKの優先接続モードを表す列挙型
typedef enum {
  CONN_PREF_BLE, // BLE優先
  CONN_PREF_USB, // USB優先
} state_conn_pref_t;

void state_set_system(state_system_t new_state);
state_system_t state_get_system(void);

void state_set_connection_preference(state_conn_pref_t pref);
state_conn_pref_t state_get_connection_preference(void);

void state_switch_connection_preference(state_conn_pref_t pref);
void state_refresh_runtime(void);

#endif // PWMK_STATE_H
