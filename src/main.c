#include <hardware/i2c.h>
#include <pico/stdlib.h>

#if PWMK_ENABLE_BLE
#include <pico/cyw43_arch.h>
#endif

#include "ble/ble.h"
#include "keyboard/code.h"
#include "keyboard/event.h"
#include "keyboard/matrix_scan.h"
#include "led/led.h"
#include "peripheral/peripheral.h"
#include "settings/board.h"
#include "settings/keymap.h"
#include "settings/settings.h"
#include "settings/users.h"
#include "state/state.h"
#include "usb/usb_hid.h"

#ifndef DEBUG_MAIN
#define DEBUG_MAIN 0
#endif

#if DEBUG_MAIN
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

#ifndef PWMK_ENABLE_USB
#define PWMK_ENABLE_USB 1
#endif

#ifndef PWMK_ENABLE_BLE
#define PWMK_ENABLE_BLE 1
#endif

#if PWMK_ENABLE_BLE
static async_at_time_worker_t pwmk_worker;
#endif
static absolute_time_t last_activity_time;
static bool requested_deep_sleep;

/**
 * @brief 1ms周期で実行する事実上のメインループ処理。
 */
static void pwmk_process_tick(void) {
  // キーマトリクス処理を実行
  matrix_process();

  // 定期処理を実行
  event_process_periodic();

  // 接続モードとトランスポートの状態を取得
  connection_preference_t connection_pref = state_get_connection_preference();
  bool usb_active = usb_hid_is_active();
  bool ble_enabled = ble_is_enabled();
  bool ble_connected = ble_is_connected();

  // USB優先時、USB接続状態に応じてBLEを動的に制御
#if PWMK_ENABLE_USB && PWMK_ENABLE_BLE
  if (connection_pref == CONN_PREF_USB) {
    if (usb_active && ble_enabled) {
      ble_power_set(false); // USB接続中はBLEをOFFにして省電力
    } else if (!usb_active && !ble_enabled) {
      ble_power_set(true); // USB未接続時はBLEをONにしてフォールバック
    }
  }
#endif

  // トランスポートの決定
  bool use_ble, use_usb;
  if (connection_pref == CONN_PREF_BLE) {
    use_ble = ble_connected;
    use_usb = !ble_connected && usb_active;
  } else {
    use_usb = usb_active;
    use_ble = !usb_active && ble_connected;
  }

  // USB定期処理
#if PWMK_ENABLE_USB
  if (!use_ble) {
    usb_hid_task();
  }
#endif

  // BLE定期処理
#if PWMK_ENABLE_BLE
  if (ble_enabled) {
    ble_poll();
  }
#endif

  bool has_activity = false;

  // 周辺機器のイベント処理（I2C通信を含むため必要な時のみ実行）
  if (peripheral_require_event_processing()) {
    peripheral_process_events();
    has_activity = true;
  }

  // HIDイベントがあればアクティブなトランスポートにレポート送信を要求
  if (event_has_event()) {
    has_activity = true;
    if (use_ble) {
      ble_request_can_send();
    } else if (use_usb) {
      usb_hid_send_reports();
    }
  }

  // アクティビティがあればタイマーをリセット
  if (has_activity) {
    last_activity_time = get_absolute_time();
  }

  // ディープスリープチェック
  if (absolute_time_diff_us(last_activity_time, get_absolute_time()) >
      DEEP_SLEEP_TIMEOUT_US) {
    requested_deep_sleep = true;
  }
}

/**
 * @brief CYW43のasync_context向け1ms定期ワーカー。
 */
#if PWMK_ENABLE_BLE
static void pwmk_worker_process(async_context_t *context,
                                async_at_time_worker_t *worker) {
  pwmk_process_tick();
  async_context_add_at_time_worker_in_ms(context, worker, 1);
}
#endif

/**
 * @brief メイン関数 エントリーポイント
 * @return 0
 */
int main() {
  state_set_system(STATE_BOOTING);
  stdio_init_all();

#if DEBUG_MAIN
  sleep_ms(2000); // UARTデバッグ用: 接続待ち
#endif
  DEBUG_PRINT("pwmk v1 start\n");

  // LEDの初期化
  led_init(GPIO_LED_PIN, LED_BRIGHTNESS);
  state_set_system(STATE_SYS_INIT);

  // 設定の初期化
  settings_init();

  // マトリクススキャン初期化
  matrix_init();

  // イベント処理を初期化
  event_init();

  // BLEの初期化
#if PWMK_ENABLE_BLE
  if (cyw43_arch_init()) {
    DEBUG_PRINT("failed to initialise cyw43_arch\n");
    return -1;
  }
  ble_setup();
  ble_power_set(true);
  state_set_system(STATE_BLE_INIT);
#endif

  // マトリクス以外の周辺機器の初期化
  peripheral_init();

  // USB HIDの初期化
#if PWMK_ENABLE_USB
  usb_hid_init();
#endif

  // 初期化完了
  state_set_system(STATE_INIT_COMPLETE);
  state_refresh_runtime();

  // アクティビティタイマー初期化
  last_activity_time = get_absolute_time();

  // 定期処理ワーカーをasync_contextに登録
#if PWMK_ENABLE_BLE
  pwmk_worker.do_work = pwmk_worker_process;
  async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(),
                                         &pwmk_worker, 1);
#endif

  // メインループ
  requested_deep_sleep = false;
  while (true) {
#if PWMK_ENABLE_BLE
    async_context_poll(cyw43_arch_async_context());
    if (requested_deep_sleep) {
      state_set_system(STATE_DEEP_SLEEP);
    }
    async_context_wait_for_work_until(cyw43_arch_async_context(),
                                      at_the_end_of_time);
#else
    pwmk_process_tick();
    if (requested_deep_sleep) {
      state_set_system(STATE_DEEP_SLEEP);
    }
    sleep_ms(1);
#endif
  }

  state_set_system(STATE_RESET);
  return 0;
}
