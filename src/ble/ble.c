#include <btstack.h>
#include <inttypes.h>
#include <pico/unique_id.h>
#include <pwmk.h>
#include <stdio.h>
#include <string.h>

#include "ble/advertising_data.h"
#include "ble/ble.h"
#include "ble/le_device_db_tlv_custom.h"
#include "keyboard/event.h"
#include "state/state.h"

#ifndef DEBUG_BLE
#define DEBUG_BLE 0
#endif

#if DEBUG_BLE
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

// --------------------------------
// 関数宣言
// --------------------------------
static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size);
static void send_report();
static void ble_apply_selected_slot_address(void);
static void ble_resume_advertising(void);

// --------------------------------
// BLE系変数定義
// --------------------------------
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static uint8_t battery = 100;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static bool ble_enabled = false;
static bool ble_restart_pending = false;

// --------------------------------
// 関数定義
// --------------------------------

/**
 * @brief BLEの初期化を行う。
 */
void ble_setup(void) {
  DEBUG_PRINT("BLE setup\n");

  // Initialize L2CAP
  l2cap_init();

  // Initialize Security Manager
  sm_init();
  sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
  sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION |
                                     SM_AUTHREQ_BONDING);

  // Initialize GATT Server
  att_server_init(profile_data, NULL, NULL);

  // Initialize GATT Services
  battery_service_server_init(battery);
  device_information_service_server_init();

  // Initialize HID Device
  hids_device_init(0, hid_descriptor, hid_descriptor_len);

  // Initialize Advertisement
  uint16_t adv_int_min = 0x0030;
  uint16_t adv_int_max = 0x0030;
  uint8_t adv_type = 0;
  bd_addr_t null_addr;
  memset(null_addr, 0, 6);
  ble_apply_selected_slot_address();
  gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0,
                                null_addr, 0x07, 0x00);
  gap_advertisements_set_data(adv_data_len, (uint8_t *)adv_data);
  gap_advertisements_enable(1);

  // Initialize Event Handlers
  hci_event_callback_registration.callback = &packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);
  sm_event_callback_registration.callback = &packet_handler;
  sm_add_event_handler(&sm_event_callback_registration);
  hids_device_register_packet_handler(packet_handler);

  DEBUG_PRINT("BLE setup complete\n");
}

/**
 * @brief btstackの電源モードを設定する。
 *        OFF時は接続中のBLEも切断される。
 * @param power trueでON、falseでOFF
 */
void ble_power_set(bool power) {
  if (power == ble_enabled) {
    return;
  }

  ble_enabled = power;
  if (power) {
    hci_power_control(HCI_POWER_ON);
  } else {
    if (con_handle != HCI_CON_HANDLE_INVALID) {
      gap_disconnect(con_handle);
    }
    con_handle = HCI_CON_HANDLE_INVALID;
    hci_power_control(HCI_POWER_OFF);
  }

  DEBUG_PRINT("BLE Power set to %s\n", power ? "ON" : "OFF");
}

/**
 * @brief btstackの処理ワーカーを起動する。
 */
void ble_poll(void) { btstack_run_loop_poll_data_sources_from_irq(); }

/**
 * @brief BLEが接続中かどうかを返す。
 * @return BLE接続中はtrue、それ以外はfalse
 */
bool ble_is_connected(void) { return con_handle != HCI_CON_HANDLE_INVALID; }

/**
 * @brief BLEが有効かどうかを返す。
 * @return BLE有効時はtrue、それ以外はfalse
 */
bool ble_is_enabled(void) { return ble_enabled; }

/**
 * @brief BLE HIDレポートの送信許可を要求する。
 *        btstackのコールバックで実際の送信が行われる。
 */
void ble_request_can_send(void) {
  if (con_handle != HCI_CON_HANDLE_INVALID) {
    hids_device_request_can_send_now_event(con_handle);
  }
}

/**
 * @brief 現在のBLE接続を切断し、再接続/再ペアリング待ち状態に戻す。
 */
void ble_reconnect(void) {
  if (!ble_enabled) {
    return;
  }

  ble_restart_pending = true;

  if (con_handle != HCI_CON_HANDLE_INVALID) {
    gap_disconnect(con_handle);
    return;
  }

  hci_power_control(HCI_POWER_OFF);
}

/**
 * @brief 指定BLEスロットへの切り替えを保留し、必要なら再接続処理を始める。
 * @param slot 切り替え先スロット番号
 * @return 操作が受付された場合はtrue、失敗時はfalse
 */
bool ble_select_slot(uint8_t slot) {
  bool updated = le_device_db_tlv_schedule_select_slot((int)slot);
  if (updated) {
    ble_reconnect();
  }
  return updated;
}

/**
 * @brief
 * 選択中BLEスロットのペアリング削除を保留し、必要なら再接続処理を始める。
 * @return 操作が受付された場合はtrue、失敗時はfalse
 */
bool ble_unpair_selected_slot(void) {
  bool updated = le_device_db_tlv_schedule_clear_selected_slot();
  if (updated) {
    ble_reconnect();
  }
  return updated;
}

// ---------------------------------
// 静的関数
// ---------------------------------

/**
 * @brief btstackのイベントハンドラ
 * @param packet_type パケットの種類
 * @param channel チャンネル
 * @param packet パケットデータ
 * @param size パケットサイズ
 */
static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size) {
  UNUSED(channel);
  UNUSED(size);

  if (packet_type != HCI_EVENT_PACKET)
    return;

  switch (hci_event_packet_get_type(packet)) {
  case BTSTACK_EVENT_STATE:
    if (ble_restart_pending) {
      switch (btstack_event_state_get_state(packet)) {
      case HCI_STATE_OFF:
        // BLE再起動のためにOFFになった場合はそのままONにする
        hci_power_control(HCI_POWER_ON);
        break;
      case HCI_STATE_WORKING:
        // BLE再起動のためにONになった場合は保留中のスロット操作を適用して再接続待ちに戻す
        le_device_db_tlv_apply_pending_slot_action();
        ble_resume_advertising();
        ble_restart_pending = false;
        state_refresh_runtime();
        break;
      default:
        break;
      }
    }
    break;
  case HCI_EVENT_DISCONNECTION_COMPLETE:
    con_handle = HCI_CON_HANDLE_INVALID;
    if (ble_restart_pending) {
      // BLE再起動のために切断された場合はOFFにしてからONにする
      hci_power_control(HCI_POWER_OFF);
    } else {
      // BLE再起動のために切断されていない場合は保留中のスロット操作を適用して再接続待ちに戻す
      le_device_db_tlv_apply_pending_slot_action();
      ble_resume_advertising();
    }
    DEBUG_PRINT("Disconnected\n");
    state_refresh_runtime();
    break;
  case SM_EVENT_JUST_WORKS_REQUEST:
    DEBUG_PRINT("Just Works requested\n");
    sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
    break;
  case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
    DEBUG_PRINT("Confirming numeric comparison: %" PRIu32 "\n",
                sm_event_numeric_comparison_request_get_passkey(packet));
    sm_numeric_comparison_confirm(
        sm_event_passkey_display_number_get_handle(packet));
    break;
  case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
    DEBUG_PRINT("Display Passkey: %" PRIu32 "\n",
                sm_event_passkey_display_number_get_passkey(packet));
    break;
  case HCI_EVENT_META_GAP:
    switch (hci_event_gap_meta_get_subevent_code(packet)) {
    case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
      // 接続完了時に接続ハンドルを更新する
      if (gap_subevent_le_connection_complete_get_status(packet) ==
          ERROR_CODE_SUCCESS) {
        con_handle =
            gap_subevent_le_connection_complete_get_connection_handle(packet);
        state_refresh_runtime();
      }
      break;
    default:
      break;
    }
    break;
  case HCI_EVENT_HIDS_META:
    switch (hci_event_hids_meta_get_subevent_code(packet)) {
    case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
      con_handle = hids_subevent_input_report_enable_get_con_handle(packet);
      DEBUG_PRINT("Report Characteristic Subscribed %s\n",
                  hids_subevent_input_report_enable_get_enable(packet) ? "ON"
                                                                       : "OFF");
      state_refresh_runtime();
      break;
    case HIDS_SUBEVENT_CAN_SEND_NOW:
      send_report();
      break;
    default:
      DEBUG_PRINT("Unhandled HIDS subevent 0x%02X\n",
                  hci_event_hids_meta_get_subevent_code(packet));
      break;
    }
    break;

  default:
    break;
  }
}

/**
 * @brief HIDレポートを送信する。
 *        送信すべきレポートがある場合にのみ送信する。
 */
static void send_report() {
  uint8_t report[HID_REPORT_SIZE_MAX] = {0};

  if (event_pop_keyboard_report(report)) {
    hids_device_send_input_report_for_id(con_handle, KEYBOARD_REPORT_ID, report,
                                         HID_KEYBOARD_REPORT_SIZE);
  }

  if (event_pop_consumer_report(report)) {
    hids_device_send_input_report_for_id(con_handle, CONSUMER_REPORT_ID, report,
                                         HID_CONSUMER_REPORT_SIZE);
  }

  if (event_pop_mouse_report(report)) {
    hids_device_send_input_report_for_id(con_handle, MOUSE_REPORT_ID, report,
                                         HID_MOUSE_REPORT_SIZE);
  }
}

/**
 * @brief 選択中スロットに応じてAdvertising用の自己アドレスを設定する。
 */
static void ble_apply_selected_slot_address(void) {
  // 選択中スロット番号とそのAdvertisingアドレス世代を取得する
  int selected_slot = le_device_db_tlv_get_selected_slot();
  uint8_t address_generation =
      le_device_db_tlv_get_slot_address_generation(selected_slot);

  // スロット固有のアドレスは、ボード固有IDとスロット番号、世代番号を組み合わせて生成する

  // ボード固有IDを取得する
  pico_unique_board_id_t board_id;
  bd_addr_t slot_addr;
  pico_get_unique_board_id(&board_id);

  // スロット固有のアドレスを生成する
  // 0x31と0x1dは適当に選んだ定数で、スロット番号と世代番号を混ぜるために使う
  for (int i = 0; i < 6; i++) {
    slot_addr[i] = board_id.id[i] ^ (uint8_t)(0x31u * (selected_slot + 1)) ^
                   (uint8_t)(0x1du * (address_generation + 1u));
  }

  // static random addressは上位2ビットが1である必要がある
  slot_addr[0] = (slot_addr[0] & 0x3Fu) | 0xC0u;
  // 各スロットのアドレスを視覚的に区別するために、全0/全1の末尾を避ける
  slot_addr[5] ^= (uint8_t)(0x55u + selected_slot + address_generation);

  // Advertising用の自己アドレスを設定する
  gap_random_address_set(slot_addr);
  gap_random_address_set_mode(GAP_RANDOM_ADDRESS_TYPE_STATIC);
  DEBUG_PRINT("BLE own address mode: static slot=%d generation=%u addr=%s\n",
              selected_slot, address_generation, bd_addr_to_str(slot_addr));
}

/**
 * @brief BLE Advertising を有効化して再接続待ちに戻す。
 */
static void ble_resume_advertising(void) {
  if (!ble_enabled) {
    return;
  }

  ble_apply_selected_slot_address();
  gap_advertisements_enable(0);
  gap_advertisements_enable(1);
}
