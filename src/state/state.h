#ifndef PWMK_STATE_H
#define PWMK_STATE_H

#include <stdbool.h>
#include <stdint.h>

// PWMKの定常状態を表す列挙型
typedef enum {
  STATE_NONE,                  // 定常状態なし
  STATE_BOOTING,               // ブート中
  STATE_SYS_INIT,              // システム初期化中
  STATE_BLE_INIT,              // BLE初期化中
  STATE_INIT_COMPLETE,         // 全初期化完了
  STATE_USB_WAITING,           // USB接続待機中
  STATE_BLE_WAITING,           // BLE接続待機中
  STATE_BLE_CONNECTED,         // BLE接続済
  STATE_USB_CONNECTED,         // USB接続済
  STATE_BOOTLOADER,            // ブートローダーモード
  STATE_DEEP_SLEEP,            // ディープスリープモード
  STATE_INIT_ERROR_PERIPHERAL, // 周辺機器初期化エラー
  STATE_INIT_ERROR_BLE,        // BLE初期化エラー
  STATE_INIT_ERROR_SETTINGS,   // 設定初期化エラー
  STATE_STEADY_COUNT,
} state_steady_t;

// PWMKの一時状態を表す列挙型
typedef enum {
  STATE_TEMP_NONE,                // 一時状態なし
  STATE_TEMP_SETTINGS_SAVING,     // 設定保存中
  STATE_TEMP_SETTINGS_PERSISTED,  // 設定保存完了
  STATE_TEMP_SETTINGS_RAM_ONLY,   // 設定保存失敗（RAMのみ反映）
  STATE_TEMP_SETTINGS_FAILED,     // 設定保存失敗（RAM反映も失敗）
  STATE_TEMP_VIAL_UNLOCKING,      // VIALアンロック中
  STATE_TEMP_VIAL_UNLOCKED,       // VIALアンロック完了
  STATE_TEMP_VIAL_LOCKED,         // VIALロック中の操作拒否
  STATE_TEMP_CONNECTION_SWITCHED, // 接続モード切替完了
  STATE_TEMP_BLE_SLOT_CHANGED,    // BLEスロット切替完了
} state_temporary_t;

// PWMKの優先接続モードを表す列挙型
typedef enum {
  CONN_PREF_BLE, // BLE優先
  CONN_PREF_USB, // USB優先
} state_conn_pref_t;

void state_request_steady_state(state_steady_t state);
state_steady_t state_get_steady_state(void);
void state_process_periodic(void);

void state_set_temporary_state(state_temporary_t state);
state_temporary_t state_get_temporary_state(void);
void state_clear_temporary_state(void);

void state_set_connection_preference(state_conn_pref_t pref);
state_conn_pref_t state_get_connection_preference(void);

void state_switch_connection_preference(state_conn_pref_t pref);

#endif // PWMK_STATE_H
