#include "keyboard/event_platform.h"
#include "ble/ble.h"
#include "debug.h"
#include "keyboard/code.h"
#include "state/state.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef DEBUG_EVENT
#define DEBUG_EVENT 0
#endif

#if DEBUG_EVENT
#define DEBUG_PRINT(...) pwmk_debug_printf("EVENT", __VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)(0))
#endif

/*
 * 公開関数
 */

/**
 * @brief プラットフォーム固有の処理が必要になる特殊キーコードの処理
 * @param icode キーコード
 * @param pressed 押下状態
 * @return 処理された場合はtrue、処理されなかった場合はfalse
 */
bool event_platform_process(icode_t icode, bool pressed) {
  if (!code_icode_is_valid(icode)) {
    return false;
  }

  // ISC_BOOTが押されたらブートモードでリセット
  if (icode == ISC_BOOT && pressed) {
    state_request_steady_state(STATE_BOOTLOADER);
    return true;
  }

  // 接続モード切替コードの処理
  if (pressed && (icode == ISC_CONN_TOGGLE || icode == ISC_CONN_USB ||
                  icode == ISC_CONN_BLE)) {
    state_conn_pref_t new_pref;

    switch (icode) {
    case ISC_CONN_TOGGLE:
      new_pref = (state_get_connection_preference() == CONN_PREF_USB)
                     ? CONN_PREF_BLE
                     : CONN_PREF_USB;
      break;
    case ISC_CONN_USB:
      new_pref = CONN_PREF_USB;
      break;
    case ISC_CONN_BLE:
      new_pref = CONN_PREF_BLE;
      break;
    default:
      return false;
    }

    state_switch_connection_preference(new_pref);
    state_set_temporary_state(STATE_TEMP_CONNECTION_SWITCHED);
    return true;
  }

  // BLEスロット系コードの処理
  if (pressed && (icode == ISC_BLE_UNPAIR || icode == ISC_BLE_SLOT_1 ||
                  icode == ISC_BLE_SLOT_2 || icode == ISC_BLE_SLOT_3 ||
                  icode == ISC_BLE_SLOT_4)) {
    bool ble_slot_updated = false;

    DEBUG_PRINT("BLE slot requested: icode=0x%04X\n", icode);

    switch (icode) {
    case ISC_BLE_UNPAIR:
      ble_slot_updated = ble_unpair_selected_slot();
      DEBUG_PRINT("BLE unpair result=%d\n", ble_slot_updated ? 1 : 0);
      break;
    case ISC_BLE_SLOT_1:
      ble_slot_updated = ble_select_slot(0);
      DEBUG_PRINT("BLE slot 1 result=%d\n", ble_slot_updated ? 1 : 0);
      break;
    case ISC_BLE_SLOT_2:
      ble_slot_updated = ble_select_slot(1);
      DEBUG_PRINT("BLE slot 2 result=%d\n", ble_slot_updated ? 1 : 0);
      break;
    case ISC_BLE_SLOT_3:
      ble_slot_updated = ble_select_slot(2);
      DEBUG_PRINT("BLE slot 3 result=%d\n", ble_slot_updated ? 1 : 0);
      break;
    case ISC_BLE_SLOT_4:
      ble_slot_updated = ble_select_slot(3);
      DEBUG_PRINT("BLE slot 4 result=%d\n", ble_slot_updated ? 1 : 0);
      break;
    default:
      break;
    }
    if (ble_slot_updated) {
      state_set_temporary_state(STATE_TEMP_BLE_SLOT_CHANGED);
    }
    return ble_slot_updated;
  }

  return false;
}
