#include <hardware/i2c.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "pinnacle/pinnacle.h"

#ifndef DEBUG_PINNACLE
#define DEBUG_PINNACLE 0
#endif

#if DEBUG_PINNACLE
#define DEBUG_PRINT(...) pwmk_debug_printf("PINNACLE", __VA_ARGS__)
#define DEBUG_DUMP(...) pwmk_debug_hexdump("PINNACLE", __VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)(0))
#define DEBUG_DUMP(...) ((void)(0))
#endif

#define PINNACLE_DEFAULT_SENSITIVITY PINNACLE_SENSITIVITY_MOST_SENSITIVE
#define PINNACLE_I2C_TIMEOUT_US 10000

static i2c_inst_t *i2c = NULL;
static uint8_t dr_pin = 0;
static uint8_t speed_lookup_table[256];
static pinnacle_rotate_t rotation = PINNACLE_ROTATE_0;
static bool i2c_access_failed = false;

/*
 * 内部関数宣言
 */

static void _rap_write_bytes(uint8_t address, uint8_t count,
                             uint8_t values[count]);
static void _rap_write(uint8_t address, uint8_t value);
static void _rap_read_bytes(uint8_t address, uint8_t count,
                            uint8_t read_buffer[count]);
static uint8_t _rap_read(uint8_t address);
static void _era_write(uint16_t address, uint8_t data);
static void _era_read_bytes(uint16_t address, uint16_t count,
                            uint8_t read_buffer[count]);
static void _wait_for_release_before_calibration(void);

/*
 * 公開関数
 */

/**
 * @brief  Cirque Pinnacle トラックパッドを初期化します。
 *         I2Cインスタンスと対応して、scl_pinとsda_pinは使用できるピンが限られていますので注意してください。
 * @param i2c_inst 使用するI2Cインスタンス
 * @param scl_pin I2CのSCLピン番号
 * @param sda_pin I2CのSDAピン番号
 * @param data_ready_pin データレディピンの番号
 * @return true: 初期化成功、false: 初期化失敗
 */
bool pinnacle_init(i2c_inst_t *i2c_inst, uint8_t scl_pin, uint8_t sda_pin,
                   uint8_t data_ready_pin) {
  i2c_access_failed = false;
  i2c = i2c_inst;
  dr_pin = data_ready_pin;
  pinnacle_set_speed(1.0f, 1.0f);

  // I2Cの初期化
  i2c_init(i2c, PINNACLE_I2C_BAUD);
  gpio_set_function(sda_pin, GPIO_FUNC_I2C);
  gpio_set_function(scl_pin, GPIO_FUNC_I2C);
  // DRピンの初期化
  gpio_init(dr_pin);
  gpio_set_dir(dr_pin, GPIO_IN);
  // 初期化完了を待つ
  sleep_ms(1);

  // ターゲットデバイスが接続されていることを確認
  uint8_t firmware[2] = {0};
  _rap_read_bytes(PINNACLE_I2C_FIRMWARE_ID, 2, firmware);
  DEBUG_PRINT("firmware ID: %02x %02x\n", firmware[0], firmware[1]);
  if (firmware[0] != 0x07 || firmware[1] != 0x3A) {
    return false;
  }

#if DEBUG_PINNACLE
  // レジスタダンプ
  uint8_t register_dump[32] = {0};
  _rap_read_bytes(0x00, 32, register_dump);
#endif

  // 初期化シーケンス
  // システムをリセット
  _rap_write(PINNACLE_I2C_SYS_CONFIG, 0x01);
  DEBUG_PRINT("reset\n");
  while (!pinnacle_check_DR()) {
    sleep_ms(1);
  }
  _rap_write(PINNACLE_I2C_STATUS, 0x00);
  DEBUG_PRINT("reset complete\n");

  DEBUG_PRINT("init start\n");
  // システム設定を初期化
  _rap_write(PINNACLE_I2C_SYS_CONFIG, 0x00);
  _rap_write(PINNACLE_I2C_FEED_CONFIG_1, 0x00);
  _rap_write(PINNACLE_I2C_FEED_CONFIG_2, 0x00);
  // フィードレートを100Hzに設定
  _rap_write(PINNACLE_I2C_SAMPLE_RATE, 100);
  // Z-idle packets の数を設定（リフトオフ検出後に送信される）
  _rap_write(PINNACLE_I2C_Z_IDLE, 30);

  // ステータスフラグをクリア
  while (pinnacle_check_DR()) {
    _rap_write(PINNACLE_I2C_STATUS, 0x00);
  }

  // 感度設定
  uint8_t sensitivity = 0;
  _era_read_bytes(0x0187, 1, &sensitivity);
  _era_write(0x0187,
             (sensitivity & 0x3F) | (PINNACLE_DEFAULT_SENSITIVITY << 6));

  // 復帰トリガー直後の接触中キャリブレーションを避ける
  _wait_for_release_before_calibration();

  // キャリブレーション
  _rap_write(PINNACLE_I2C_CAL_CONFIG, 0b00011111);
  while (!pinnacle_check_DR()) {
    sleep_ms(1);
  }
  _rap_write(PINNACLE_I2C_STATUS, 0x00);

  // フィード機能を有効にしてrelativeモードに設定
  uint8_t feedConfig1 = 0b00000001;
  _rap_write(PINNACLE_I2C_FEED_CONFIG_1, feedConfig1);

  // スリープへ移行することを許可
  _rap_write(PINNACLE_I2C_SYS_CONFIG, 0x04);
  _rap_write(PINNACLE_I2C_SLEEP_INTERVAL, 0x80);
  _rap_write(PINNACLE_I2C_SLEEP_TIMER, 0x08);
  DEBUG_PRINT("init complete\n");

  return !i2c_access_failed;
}

/**
 * @brief トラックパッドの速度を設定します。
 * deltaに対して、delta^accel * speedの移動量が報告されるようになります。
 * @param accel 加速度
 * @param speed 速度
 */
void pinnacle_set_speed(float accel, float speed) {
  for (int i = 0; i < 256; i++) {
    float value = powf(i, accel) * speed;
    if (i != 0 && value < 1.0f) {
      value = 1.0f; // 0以外の値は最低でも1になるようにする
    }
    speed_lookup_table[i] = (uint8_t)fminf(fmaxf(value, 0.0f), 127.0f);
  }
}

/**
 * @brief トラックパッドの回転を設定します。
 * @param rot 回転 (PINNACLE_ROTATE_xxx)
 */
void pinnacle_set_rotation(pinnacle_rotate_t rot) { rotation = rot; }

/**
 * @brief データレディピンの状態を取得する。
 * @return true:データレディ、false:データ未準備
 */
bool pinnacle_check_DR() { return gpio_get(dr_pin); }

/**
 * @brief トラックパッドからデータを読み取ります。
 * @param data 読み取ったデータを格納する構造体へのポインタ
 * @return true:データ取得成功、false:データ未準備
 */
bool pinnacle_read_data(pinnacle_data_t *data) {
  i2c_access_failed = false;

  // i2cが初期化されていない場合はfalseを返す
  if (i2c == NULL) {
    return false;
  }

  // データレディでない場合はfalseを返す
  if (!pinnacle_check_DR()) {
    return false;
  }

  // データパケットを読み込み
  uint8_t packet_data[4] = {0};
  _rap_read_bytes(PINNACLE_I2C_PACKET_BYTE_0, 4, packet_data);
  // ステータスフラグをクリア
  _rap_write(PINNACLE_I2C_STATUS, 0x00);

  // relativeモードのデータ解析
  data->buttons = packet_data[0] & 0x07; // 下位3ビットがボタン情報
  int8_t dx = (int8_t)packet_data[1];    // X軸の移動量
  int8_t dy = -(int8_t)packet_data[2];   // Y軸の移動量 (pinnacleのY軸は上下逆)
  data->wDelta = (int8_t)packet_data[3]; // スクロールホイール量

  // 回転反映
  switch (rotation) {
  case PINNACLE_ROTATE_0:
    data->xDelta = dx;
    data->yDelta = dy;
    break;
  case PINNACLE_ROTATE_90:
    data->xDelta = -dy;
    data->yDelta = dx;
    break;
  case PINNACLE_ROTATE_180:
    data->xDelta = -dx;
    data->yDelta = -dy;
    break;

  case PINNACLE_ROTATE_270:
    data->xDelta = dy;
    data->yDelta = -dx;
    break;
  }

  // 速度調整
  data->xDelta = (int8_t)speed_lookup_table[(uint8_t)abs(data->xDelta)] *
                 (data->xDelta < 0 ? -1 : 1);
  data->yDelta = (int8_t)speed_lookup_table[(uint8_t)abs(data->yDelta)] *
                 (data->yDelta < 0 ? -1 : 1);

  return !i2c_access_failed;
}

/*
 * 内部関数
 */

/**
 * @brief RAP(register access protocol)でレジスタに書き込みを行う。
 * @param address 書き込み開始レジスタアドレス
 * @param count 書き込みバイト数
 * @param values 書き込みデータ配列
 */
static void _rap_write_bytes(uint8_t address, uint8_t count,
                             uint8_t values[count]) {
  DEBUG_PRINT("I2C-W %d bytes to address 0x%02X\n", count, address);
  DEBUG_DUMP("I2C-W", values, count);
  uint8_t write_buffer[count * 2];
  for (uint8_t i = 0; i < count; ++i) {
    write_buffer[i * 2] = 0x80 | (address + i);
    write_buffer[i * 2 + 1] = values[i];
  }
  int written_bytes = i2c_write_timeout_us(i2c, PINNACLE_I2C_TARGET_ADDRESS,
                                           write_buffer, sizeof(write_buffer),
                                           false, PINNACLE_I2C_TIMEOUT_US);
#if DEBUG_PINNACLE
  DEBUG_PRINT("I2C-W %d bytes done\n", written_bytes);
  if (written_bytes != (int)sizeof(write_buffer)) {
    DEBUG_PRINT("I2C-W ERROR detected\n");
  }
#endif
  if (written_bytes != (int)sizeof(write_buffer)) {
    i2c_access_failed = true;
  }
}

/**
 * @brief RAPでレジスタに1バイト書き込みを行う。
 * @param address 書き込みレジスタアドレス
 * @param value 書き込みデータ
 */
static void _rap_write(uint8_t address, uint8_t value) {
  _rap_write_bytes(address, 1, &value);
}

/**
 * @brief RAPでレジスタから読み込みを行う。
 * @param address 読み込み開始レジスタアドレス
 * @param count 読み込みバイト数
 * @param read_buffer 読み込みデータ配列
 */
static void _rap_read_bytes(uint8_t address, uint8_t count,
                            uint8_t read_buffer[count]) {
  DEBUG_PRINT("I2C-R %d bytes from address 0x%02X\n", count, address);

  address |= 0xA0;
  int written_bytes =
      i2c_write_timeout_us(i2c, PINNACLE_I2C_TARGET_ADDRESS, &address,
                           sizeof(address), false, PINNACLE_I2C_TIMEOUT_US);
  if (written_bytes != (int)sizeof(address)) {
    i2c_access_failed = true;
  }

  int read_bytes =
      i2c_read_timeout_us(i2c, PINNACLE_I2C_TARGET_ADDRESS, read_buffer, count,
                          false, PINNACLE_I2C_TIMEOUT_US);

#if DEBUG_PINNACLE
  DEBUG_PRINT("I2C-R %d bytes done\n", read_bytes);
  if (read_bytes == (int)count) {
    DEBUG_DUMP("I2C-R", read_buffer, read_bytes);
  } else {
    DEBUG_PRINT("I2C-R ERROR detected\n");
  }
#endif

  if (read_bytes != (int)count) {
    i2c_access_failed = true;
  }
}

/**
 * @brief RAPでレジスタから1バイト読み込みを行う。
 * @param address 読み込みレジスタアドレス
 */
static uint8_t _rap_read(uint8_t address) {
  uint8_t read_buffer[1] = {0};
  _rap_read_bytes(address, 1, read_buffer);
  return read_buffer[0];
}

/**
 * @brief ERA(Extended Register Access)でレジスタに1バイト書き込みを行う。
 */
static void _era_write(uint16_t address, uint8_t data) {
  // データフィードを無効化
  uint8_t feedConfig1 = _rap_read(PINNACLE_I2C_FEED_CONFIG_1);
  _rap_write(PINNACLE_I2C_FEED_CONFIG_1, feedConfig1 & 0xFE);

  // ERA用のレジスタに書き込み
  _rap_write(PINNACLE_I2C_ERA_VALUE, data);
  _rap_write(PINNACLE_I2C_ERA_ADDR_HIGH, (uint8_t)(address >> 8));
  _rap_write(PINNACLE_I2C_ERA_ADDR_LOW, (uint8_t)(address & 0x00FF));
  // ERA書き込み開始
  _rap_write(PINNACLE_I2C_ERA_CONTROL, 0x02);

  // 書き込み完了待ち
  while (_rap_read(PINNACLE_I2C_ERA_CONTROL) != 0x00) {
    sleep_us(1);
  }
  DEBUG_PRINT("I2C-W ERA 0x%04x <= 0x%02x\n", address, data);

  // ステータスフラグをクリア
  _rap_write(PINNACLE_I2C_STATUS, 0x00);

  // データフィードを元に戻す
  _rap_write(PINNACLE_I2C_FEED_CONFIG_1, feedConfig1);
}

/**
 * @brief ERAでレジスタから読み込みを行う。
 * @param address 読み込み開始レジスタアドレス
 * @param count 読み込みバイト数
 * @param read_buffer 読み込みデータ配列
 */
static void _era_read_bytes(uint16_t address, uint16_t count,
                            uint8_t read_buffer[count]) {
  // データフィードを無効化
  uint8_t feedConfig1 = _rap_read(PINNACLE_I2C_FEED_CONFIG_1);
  _rap_write(PINNACLE_I2C_FEED_CONFIG_1, feedConfig1 & 0xFE);

  // ERA用のレジスタに書き込み
  _rap_write(PINNACLE_I2C_ERA_ADDR_HIGH, (uint8_t)(address >> 8));
  _rap_write(PINNACLE_I2C_ERA_ADDR_LOW, (uint8_t)(address & 0x00FF));

  for (uint16_t i = 0; i < count; i++) {
    // ERA読み込み開始
    _rap_write(PINNACLE_I2C_ERA_CONTROL, 0x05);
    // 読み込み完了待ち
    while (_rap_read(PINNACLE_I2C_ERA_CONTROL) != 0x00) {
      sleep_us(1);
    }

    // データ読み込み
    read_buffer[i] = _rap_read(PINNACLE_I2C_ERA_VALUE);
    DEBUG_PRINT("I2C-R ERA 0x%04x => 0x%02x\n", address, read_buffer[i]);

    // ステータスフラグをクリア
    _rap_write(PINNACLE_I2C_STATUS, 0x00);
  }
  // データフィードを元に戻す
  _rap_write(PINNACLE_I2C_FEED_CONFIG_1, feedConfig1);
}

/**
 * @brief 復帰直後の接触残りを避けるため、一定時間DRが静かな状態を待つ。
 */
static void _wait_for_release_before_calibration(void) {
  absolute_time_t start = get_absolute_time();
  absolute_time_t released_since = {0};
  bool release_started = false;

  while (absolute_time_diff_us(start, get_absolute_time()) <
         (int64_t)PINNACLE_CALIBRATION_RELEASE_TIMEOUT_MS * 1000) {
    if (pinnacle_check_DR()) {
      // pendingデータを捨て、接触/動作が落ち着くのを待つ
      _rap_write(PINNACLE_I2C_STATUS, 0x00);
      release_started = false;
    } else {
      if (!release_started) {
        released_since = get_absolute_time();
        release_started = true;
      } else if (absolute_time_diff_us(released_since, get_absolute_time()) >=
                 (int64_t)PINNACLE_CALIBRATION_RELEASE_WAIT_MS * 1000) {
        return;
      }
    }

    sleep_ms(1);
  }
}
