#if PWMK_ENABLE_USB

#include <string.h>
#include <tusb.h>

#include "hid/hid.h"
#include "keyboard/event.h"
#include "state/state.h"
#include "usb/usb_descriptors.h"
#include "usb/usb_hid.h"
#include "vial/vial.h"

static void usb_hid_send_report_chain(void);
static bool usb_hid_report_chain_active;

// --------------------------------
// 公開関数
// --------------------------------

/**
 * @brief USB HIDデバイスの初期化を行う。
 */
void usb_hid_init(void) {
  usb_descriptors_init();
  tud_init(BOARD_TUD_RHPORT);
}

/**
 * @brief USB HIDデバイスの終了処理を行う。
 */
void usb_hid_deinit(void) { tud_deinit(BOARD_TUD_RHPORT); }

/**
 * @brief USB HIDのデバイスのタスク処理を行う。
 *        メインループから定期的に呼び出す必要がある。
 */
void usb_hid_task(void) { tud_task(); }

/**
 * @brief USB HIDレポートの送信を試みる。
 *        USBがマウント済みかつHIDが準備完了の場合にレポートチェーンを開始する。
 */
void usb_hid_send_reports(void) {
  if (!tud_mounted() || !tud_hid_n_ready(USB_HID_INSTANCE_INPUT) ||
      !event_has_event()) {
    return;
  }
  usb_hid_report_chain_active = true;
  usb_hid_send_report_chain();
}

/**
 * @brief
 * USBが接続済みかつHID通信準備完了かどうかを返す。
 * @return USBが接続済みかつHID通信準備完了な場合はtrue、それ以外はfalse
 */
bool usb_hid_is_active(void) { return tud_mounted(); }

// --------------------------------
// TinyUSBデバイスコールバック
// --------------------------------

/**
 * @brief USBデバイスがマウントされた時のコールバック。
 */
void tud_mount_cb(void) { state_refresh_runtime(); }

/**
 * @brief USBデバイスがアンマウントされた時のコールバック。
 */
void tud_umount_cb(void) { state_refresh_runtime(); }

/**
 * @brief USBバスがサスペンドされた時のコールバック。
 */
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }

/**
 * @brief USBバスがレジュームされた時のコールバック。
 */
void tud_resume_cb(void) {}

// --------------------------------
// TinyUSB HIDコールバック
// --------------------------------

/**
 * @brief HIDレポート送信完了コールバック。
 *        レポートチェーンの次のレポートを送信する。
 */
void tud_hid_report_complete_cb(uint8_t _instance, uint8_t const *report,
                                uint16_t len) {
  (void)report;
  (void)len;

  if (_instance == USB_HID_INSTANCE_VIAL) {
    return;
  }

  if (_instance != USB_HID_INSTANCE_INPUT || !usb_hid_report_chain_active) {
    return;
  }

  usb_hid_send_report_chain();
}

/**
 * @brief GET_REPORTリクエストのコールバック。
 */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;
  return 0;
}

/**
 * @brief SET_REPORTリクエストのコールバック。
 *        キーボードLED（CapsLock等）の処理に使用可能。
 */
void tud_hid_set_report_cb(uint8_t _instance, uint8_t _report_id,
                           hid_report_type_t _report_type,
                           uint8_t const *_buffer, uint16_t _bufsize) {

  // VIAL用のHID通信ではない場合は無視する
  if (_instance != USB_HID_INSTANCE_VIAL) {
    return;
  }

  // パケットの長さが規定+1の場合は先頭バイトを無視する
  const uint8_t *packet = _buffer;
  uint16_t packet_size = _bufsize;
  if (packet_size == VIAL_PACKET_SIZE + 1) {
    if (packet[0] == 0) {
      packet++;
    }
    packet_size--;
  }

  // レポートID、レポートタイプ、パケットの長さが規定と異なる場合は無視する
  if (_report_id != 0 || _report_type != HID_REPORT_TYPE_OUTPUT ||
      packet_size != VIAL_PACKET_SIZE) {
    return;
  }

  // 受信したパケットをそのまま応答用バッファとして再利用する
  uint8_t response[VIAL_PACKET_SIZE];
  memcpy(response, packet, sizeof(response));

  // 受信したパケットを処理する
  vial_handle_packet(response);

  // 処理結果を応答として送信する
  if (!tud_hid_n_ready(USB_HID_INSTANCE_VIAL)) {
    return;
  }
  tud_hid_n_report(USB_HID_INSTANCE_VIAL, 0, response, sizeof(response));
}

// --------------------------------
// 内部関数
// --------------------------------

/**
 * @brief レポートチェーンを開始する。
 *        送信すべきレポートがあれば送信して終了する。
 *        （続きはtud_hid_report_complete_cbで処理）
 */
static void usb_hid_send_report_chain() {

  event_hid_report_t report;
  if (event_pop_hid_report(&report)) {
    tud_hid_report(report.report_id, report.data, report.size);
  } else {
    usb_hid_report_chain_active = false;
  }
}

#endif // PWMK_ENABLE_USB
