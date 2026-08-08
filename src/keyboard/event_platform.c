#include "keyboard/event_platform.h"
#include "ble/ble.h"
#include "keyboard/code.h"
#include "state/state.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef DEBUG_EVENT
#define DEBUG_EVENT 0
#endif

#if DEBUG_EVENT
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)(0))
#endif

/**
 * @brief プラットフォーム固有の処理が必要になる特殊キーコードの処理
 * @param icode キーコード
 * @param pressed 押下状態
 * @return 処理された場合はtrue、処理されなかった場合はfalse
 */
bool event_process_platform(icode_t icode, bool pressed) {
  // ISC_BOOTが押されたらブートモードでリセット
  if (icode == ISC_BOOT && pressed) {
    state_set_system(STATE_BOOTLOADER);
    return true;
  }

  // 接続モード切替コードの処理
  if (pressed && ISC_CONN_TOGGLE <= icode && icode <= ISC_CONN_BLE) {
    connection_preference_t new_pref;
    bool handled = true;

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
      handled = false;
      break;
    }

    if (handled) {
      state_switch_connection_preference(new_pref);
      return true;
    }
  }

  // BLEスロット系コードの処理
  if (pressed && ISC_BLE_UNPAIR <= icode && icode <= ISC_BLE_SLOT_4) {
    bool ble_slot_updated = false;

    DEBUG_PRINT("BLE slot op requested: icode=0x%04X\n", icode);

    switch (icode) {
    case ISC_BLE_UNPAIR:
      ble_slot_updated = ble_unpair_selected_slot();
      DEBUG_PRINT("BLE slot op: unpair selected result=%d\n",
                  ble_slot_updated ? 1 : 0);
      break;
    case ISC_BLE_SLOT_1:
      ble_slot_updated = ble_select_slot(0);
      DEBUG_PRINT("BLE slot op: select slot 1 result=%d\n",
                  ble_slot_updated ? 1 : 0);
      break;
    case ISC_BLE_SLOT_2:
      ble_slot_updated = ble_select_slot(1);
      DEBUG_PRINT("BLE slot op: select slot 2 result=%d\n",
                  ble_slot_updated ? 1 : 0);
      break;
    case ISC_BLE_SLOT_3:
      ble_slot_updated = ble_select_slot(2);
      DEBUG_PRINT("BLE slot op: select slot 3 result=%d\n",
                  ble_slot_updated ? 1 : 0);
      break;
    case ISC_BLE_SLOT_4:
      ble_slot_updated = ble_select_slot(3);
      DEBUG_PRINT("BLE slot op: select slot 4 result=%d\n",
                  ble_slot_updated ? 1 : 0);
      break;
    default:
      break;
    }
    return ble_slot_updated;
  }

  return false;
}
