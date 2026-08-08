#include <hardware/clocks.h>
#include <hardware/pll.h>
#include <hardware/regs/io_bank0.h>
#include <hardware/rosc.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <hardware/xosc.h>
#include <pico/low_power.h>
#include <pico/stdlib.h>
#include <stdio.h>

#if PWMK_ENABLE_BLE
#include <pico/cyw43_arch.h>
#endif

#include "ble/ble.h"
#include "led/led.h"
#include "settings/board.h"
#include "state/sleep.h"
#include "usb/usb_hid.h"

#ifndef DEBUG_MAIN
#define DEBUG_MAIN 0
#endif

#if DEBUG_MAIN
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)(0))
#endif

#if PICO_RP2040
/**
 * @brief マトリクス列のGPIOをドーマントウェイクに設定する。
 *        行をLOW出力に固定し、列のLOWエッジで復帰する経路を作る。
 */
static void matrix_enable_dormant_wakeup(void) {
  // 行をLOW出力に固定し、押下時に列がLOWへ落ちる経路を作る。
  for (int row = 0; row < ROWS; row++) {
    uint8_t row_pin = rows_pins[row];
    gpio_set_function(row_pin, GPIO_FUNC_SIO);
    gpio_disable_pulls(row_pin);
    gpio_put(row_pin, 0);
    gpio_set_dir(row_pin, GPIO_OUT);
  }

  // 列はプルアップ入力にし、LOWエッジでドーマント復帰を有効化する。
  for (int col = 0; col < COLS; col++) {
    uint8_t col_pin = cols_pins[col];
    gpio_set_function(col_pin, GPIO_FUNC_SIO);
    gpio_set_dir(col_pin, GPIO_IN);
    gpio_pull_up(col_pin);
    gpio_set_dormant_irq_enabled(col_pin, GPIO_IRQ_EDGE_FALL, true);
  }
}

/**
 * @brief ドーマント復帰後にマトリクス列のGPIO割り込みをクリアする。
 */
static void matrix_acknowledge_dormant_wakeup(void) {
  for (int col = 0; col < COLS; col++) {
    uint8_t col_pin = cols_pins[col];
    gpio_acknowledge_irq(col_pin, GPIO_IRQ_EDGE_FALL);
    gpio_set_dormant_irq_enabled(col_pin, GPIO_IRQ_EDGE_FALL, false);
  }
}

/**
 * @brief DORMANTを使用してディープスリープに入る。
 */
void enter_deepsleep(void) {
  // RP2040では、ディープスリープはDORMANTモードとして実装する。
  DEBUG_PRINT("entering dormant mode\n");
  prepare_deep_sleep();

  // クロックをXOSCに切り替え（PLLを停止するため）
  clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0,
                  XOSC_HZ, XOSC_HZ);
  clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF,
                  CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, XOSC_HZ,
                  XOSC_HZ);
  clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                  XOSC_HZ, XOSC_HZ);
  // USB/ADCクロックを無効化
  clock_stop(clk_usb);
  clock_stop(clk_adc);
  // PLLを無効化
  pll_deinit(pll_sys);
  pll_deinit(pll_usb);
  // ROSCを無効化
  rosc_disable();

  // どのキー押下でも復帰できるよう、マトリクス列のLOWエッジを有効化
  matrix_enable_dormant_wakeup();

  // GPIOドーマントウェイク設定 (DR pin, rising edge)
  gpio_set_dormant_irq_enabled(GPIO_DR_PIN, GPIO_IRQ_EDGE_RISE, true);

  // ドーマントモードに入る（ここで停止し、GPIO割り込みで復帰）
  xosc_dormant();

  // --- 復帰後 ---

  // ROSCを復帰
  rosc_restart();

  // IRQをクリア
  matrix_acknowledge_dormant_wakeup();
  gpio_acknowledge_irq(GPIO_DR_PIN, GPIO_IRQ_EDGE_RISE);
  gpio_set_dormant_irq_enabled(GPIO_DR_PIN, GPIO_IRQ_EDGE_RISE, false);

  // ウォッチドッグでリブート
  watchdog_reboot(0, 0, 0);

  // リブート待ち
  while (true) {
    tight_loop_contents();
  }
}

#elif PICO_RP2350

/**
 * @brief ディープスリープに入る前の準備を行う。
 */
static void prepare_deep_sleep(void) {
  // 割り込みを無効化
  disable_interrupts();

  // LEDを消灯
  led_put_rgb(0, 0, 0);

  // BLEを無効化
#if PWMK_ENABLE_BLE
  ble_power_set(false);
  gpio_put(CYW43_PIN_WL_REG_ON, false);
#endif

  // USBを無効化
#if PWMK_ENABLE_USB
  usb_hid_deinit();
#endif

  // stdio をフラッシュ
  stdio_flush();
}

/**
 * @brief PSTATEを使用してディープスリープに入る。
 */
void enter_deepsleep(void) {
  // RP2350では、ディープスリープはPSTATE(P1.7)として実装する。
  DEBUG_PRINT("entering pstate mode\n");
  prepare_deep_sleep();

  // GPIOピンの割り込み起床を設定する。
  powman_enable_gpio_wakeup(0, GPIO_DR_PIN, true, true);

  // TODO マトリクスでも起床できるようにする
  // powman_enable_gpio_wakeup(1, gpio_pin, edge, high);

  // 移行先の状態をP1.7(AON以外の全ドメイン電源OFF)に設定する
  pstate_bitset_t pstate = pstate_bitset_none();

  // デバッガからの電源要求によってパワーダウンが阻止されないようにする。
  powman_set_debug_power_request_ignored(true);
  // VREG をアンロックし、低消費電力モードへ切り替えられるようにする。
  hw_set_bits(&powman_hw->vreg_ctrl,
              POWMAN_PASSWORD_BITS | POWMAN_VREG_CTRL_UNLOCK_BITS);
  // ブートアドレスをクリアする。
  powman_hw->boot[0] = 0;
  powman_hw->boot[1] = 0;
  powman_hw->boot[2] = 0;
  powman_hw->boot[3] = 0;

  // PSTATE遷移を実行する。
  powman_set_power_state(pstate_bitset_to_powman_power_state(&pstate));

  // 電源断が実行されるまで待機する。GPIO割り込みで復帰すると電源再起動と同じ状態になる。
  while (true) {
    __wfi();
  }
#endif
}
