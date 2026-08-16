#if PWMK_ENABLE_USB

#include <string.h>
#include <tusb.h>

#include "hid/hid.h"
#include "keyboard/event.h"
#include "state/state.h"
#include "usb/usb_descriptors.h"
#include "usb/usb_hid.h"
#include "vial/vial.h"

static bool usb_hid_report_chain_active;

static uint8_t vial_buffer[VIAL_PACKET_SIZE];
static bool vial_request_pending;

/*
 * 内部関数宣言
 */

static void _usb_hid_send_report_chain(void);
static void _usb_hid_process_vial(void);

/*
 * 公開関数
 */

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
void usb_hid_task(void) {
  tud_task();
  _usb_hid_process_vial();
}

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
  _usb_hid_send_report_chain();
}

/**
 * @brief USBが接続済みかつHID通信準備完了かどうかを返す。
 * @return USBが接続済みかつHID通信準備完了な場合はtrue、それ以外はfalse
 */
bool usb_hid_is_active(void) { return tud_mounted(); }

/**
 * @brief USBデバイスがマウントされた時のコールバック。
 * @note この関数はTinyUSBのデバイスコールバック関数として使用されます。
 */
void tud_mount_cb(void) { state_refresh_runtime(); }

/**
 * @brief USBデバイスがアンマウントされた時のコールバック。
 * @note この関数はTinyUSBのデバイスコールバック関数として使用されます。
 */
void tud_umount_cb(void) {
  vial_request_pending = false;
  state_refresh_runtime();
}

/**
 * @brief USBバスがサスペンドされた時のコールバック。
 * @param remote_wakeup_en リモートウェイクアップが有効かどうか
 * @note この関数はTinyUSBのデバイスコールバック関数として使用されます。
 */
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }

/**
 * @brief USBバスがレジュームされた時のコールバック。
 * @note この関数はTinyUSBのデバイスコールバック関数として使用されます。
 */
void tud_resume_cb(void) {}

/**
 * @brief HIDレポート送信完了コールバック。
 *        レポートチェーンの次のレポートを送信する。
 * @param _instance インターフェース番号
 * @param report 送信したレポートのデータ
 * @param len 送信したレポートの長さ
 * @note この関数はTinyUSBのHIDコールバック関数として使用されます。
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

  _usb_hid_send_report_chain();
}

/**
 * @brief GET_REPORTリクエストのコールバック。
 * @param instance インターフェース番号
 * @param report_id レポートID
 * @param report_type レポートタイプ
 * @param buffer レポートデータを格納するバッファ
 * @param reqlen バッファの長さ
 * @return レポートデータの長さ
 * @note この関数はTinyUSBのHIDコールバック関数として使用されます。
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
 * @param instance インターフェース番号
 * @param report_id レポートID
 * @param report_type レポートタイプ
 * @param buffer レポートデータ
 * @param bufsize レポートデータの長さ
 * @note この関数はTinyUSBのHIDコールバック関数として使用されます。
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

  // VIAL要求はフラッシュ操作を含み得るため、メインループで処理する。
  if (vial_request_pending) {
    return;
  }
  memcpy(vial_buffer, packet, sizeof(vial_buffer));
  vial_request_pending = true;
}

/*
 * 内部関数
 */

/**
 * @brief レポートチェーンを開始する。
 *        送信すべきレポートがあれば送信して終了する。
 *        （続きはtud_hid_report_complete_cbで処理）
 */
static void _usb_hid_send_report_chain() {

  event_hid_report_t report;
  if (event_pop_hid_report(&report)) {
    tud_hid_report(report.report_id, report.data, report.size);
  } else {
    usb_hid_report_chain_active = false;
  }
}

/**
 * @brief VIAL要求を通常のメインループ文脈で処理し、応答を送信する。
 */
static void _usb_hid_process_vial(void) {
  if (!vial_request_pending) {
    return;
  }
  vial_request_pending = false;

  // 受信したパケットを処理する
  vial_handle_packet(vial_buffer);

  // 処理結果を応答として送信する
  if (!tud_hid_n_ready(USB_HID_INSTANCE_VIAL)) {
    return;
  }
  tud_hid_n_report(USB_HID_INSTANCE_VIAL, 0, vial_buffer, sizeof(vial_buffer));
}

#endif // PWMK_ENABLE_USB
