#if PWMK_ENABLE_USB

#include <stdio.h>
#include <string.h>

#include <pico/unique_id.h>

#include "keyboard/code_convert.h"
#include "keyboard/matrix_scan.h"
#include "profile/board.h"
#include "profile/keymap.h"
#include "profile/vial_definition.h"
#include "settings/settings.h"
#include "state/state.h"
#include "vial/vial.h"

#ifndef DEBUG_VIAL
#define DEBUG_VIAL 0
#endif

#if DEBUG_VIAL
#define DEBUG_PRINT_PACKET(label, packet)                                      \
  do {                                                                         \
    printf("%s: ", label);                                                     \
    for (uint8_t index = 0; index < VIAL_PACKET_SIZE; index++) {               \
      printf("0x%02X ", packet[index]);                                        \
    }                                                                          \
    printf("\n");                                                              \
  } while (0)
#else
#define DEBUG_PRINT_PACKET(...) ((void)(0))
#endif

#define VIAL_PREFIX 0xFE
#define VIAL_PROTOCOL_VERSION 6
#define VIA_PROTOCOL_VERSION 0x0009
#define VIAL_UNLOCK_HOLD_MS 5000
#define VIAL_KEYMAP_RESPONSE_HEADER_SIZE 4
#define VIA_VALUE_ID_SWITCH_MATRIX_STATE 0x03

static bool unlocked;           // アンロック状態
static bool unlock_in_progress; // アンロックが進行中かどうか
static bool unlock_combo_held;  // アンロックコンボが押されているかどうか
static uint32_t unlock_hold_started_at_ms; // アンロックを始めた時刻

/*
 * 内部関数宣言
 */

static uint16_t _read_u16_be(const uint8_t *buffer);
static uint16_t _read_u16_le(const uint8_t *buffer);
static void _write_u16_be(uint8_t *buffer, uint16_t value);
static void _write_u32_le(uint8_t *buffer, uint32_t value);
static bool _vial_unlock_combo_pressed(void);
static uint8_t _vial_update_unlock_state(void);
static void _copy_keyboard_definition_page(uint16_t page,
                                           uint8_t response[VIAL_PACKET_SIZE]);
static uint16_t _keymap_buffer_get_keycode(uint16_t key_index);
static uint8_t _keymap_buffer_get_byte(uint16_t offset);
static bool _vial_keycode_write_allowed(uint16_t keycode);
static void _write_switch_matrix_state(uint8_t response[VIAL_PACKET_SIZE]);
static void _handle_vial_command(const uint8_t request[VIAL_PACKET_SIZE],
                                 uint8_t response[VIAL_PACKET_SIZE]);
static void _handle_dynamic_keymap_get(const uint8_t request[VIAL_PACKET_SIZE],
                                       uint8_t response[VIAL_PACKET_SIZE]);
static void _handle_via_command(const uint8_t request[VIAL_PACKET_SIZE],
                                uint8_t response[VIAL_PACKET_SIZE]);

/*
 * 公開関数
 */

/**
 * @brief Vialパケットを処理する。
 * @param packet 受信したパケット
 */
void vial_handle_packet(uint8_t packet[VIAL_PACKET_SIZE]) {
  uint8_t request[VIAL_PACKET_SIZE];
  // 入力を退避してから同じ 32 バイト領域をゼロ初期化した応答として再利用する。
  memcpy(request, packet, sizeof(request));
  DEBUG_PRINT_PACKET("Vial request", request);
  memset(packet, 0, VIAL_PACKET_SIZE);

  if (request[0] == VIAL_PREFIX) {
    // 0xFE プレフィックスは Vial 固有コマンド、
    // それ以外は VIA コマンドとして解釈する。
    _handle_vial_command(request, packet);
  } else {
    _handle_via_command(request, packet);
  }

  DEBUG_PRINT_PACKET("Vial response", packet);
}

/*
 * 内部関数
 */

/**
 * @brief バッファから16ビットのビッグエンディアン値を読み取る。
 * @param buffer 読み取り元のバッファ
 * @return 16ビットの値
 */
static uint16_t _read_u16_be(const uint8_t *buffer) {
  return ((uint16_t)buffer[0] << 8) | buffer[1];
}

/**
 * @brief バッファから16ビットのリトルエンディアン値を読み取る。
 * @param buffer 読み取り元のバッファ
 * @return 16ビットの値
 */
static uint16_t _read_u16_le(const uint8_t *buffer) {
  return ((uint16_t)buffer[1] << 8) | buffer[0];
}

/**
 * @brief バッファに16ビットのビッグエンディアン値を書き込む。
 * @param buffer 書き込み先のバッファ
 * @param value 書き込む16ビットの値
 */
static void _write_u16_be(uint8_t *buffer, uint16_t value) {
  buffer[0] = (uint8_t)(value >> 8);
  buffer[1] = (uint8_t)value;
}

/**
 * @brief バッファに32ビットのリトルエンディアン値を書き込む。
 * @param buffer 書き込み先のバッファ
 * @param value 書き込む32ビットの値
 */
static void _write_u32_le(uint8_t *buffer, uint32_t value) {
  buffer[0] = (uint8_t)value;
  buffer[1] = (uint8_t)(value >> 8);
  buffer[2] = (uint8_t)(value >> 16);
  buffer[3] = (uint8_t)(value >> 24);
}

/**
 * @brief Vialのアンロックコンボが押されているかどうかを判定する。
 * @return アンロックコンボが押されている場合はtrue、それ以外はfalse。
 */
static bool _vial_unlock_combo_pressed(void) {
  for (uint8_t index = 0; index < VIAL_UNLOCK_COMBO_LENGTH; index++) {
    if (!matrix_scan_is_pressed(vial_unlock_combo[index][0],
                                vial_unlock_combo[index][1])) {
      return false;
    }
  }
  return true;
}

/**
 * @brief アンロック状態を更新する。
 * @return アンロックコンボが押されている場合は残り時間（秒）を返す。
 */
static uint8_t _vial_update_unlock_state(void) {
  // アンロック済みまたはアンロックが進行していない場合は0を返す。
  if (unlocked || !unlock_in_progress) {
    return 0;
  }

  // アンロックコンボが押されていない場合は進行をリセットして残り時間を返す。
  if (!_vial_unlock_combo_pressed()) {
    unlock_combo_held = false;
    return VIAL_UNLOCK_HOLD_MS / 1000;
  }

  uint32_t now = to_ms_since_boot(get_absolute_time());
  uint32_t elapsed_ms = now - unlock_hold_started_at_ms;

  // 押されていると判断された最初の時刻を記録する。
  if (!unlock_combo_held) {
    unlock_combo_held = true;
    unlock_hold_started_at_ms = now;
    return VIAL_UNLOCK_HOLD_MS / 1000;
  }

  // 経過時間が規定の時間を超えた場合はアンロック状態にする。
  if (elapsed_ms >= VIAL_UNLOCK_HOLD_MS) {
    unlocked = true;
    unlock_in_progress = false;
    unlock_combo_held = false;
    return 0;
  }

  // まだ時間が足りていない場合は経過時間を計算して残り時間を返す。
  return (uint8_t)((VIAL_UNLOCK_HOLD_MS - elapsed_ms + 999) / 1000);
}

/**
 * @brief キーボード定義のページをコピーする。
 * @param page コピーするページ番号
 * @param response コピー先のバッファ
 */
static void _copy_keyboard_definition_page(uint16_t page,
                                           uint8_t response[VIAL_PACKET_SIZE]) {
  // Definition Data Query の応答はヘッダなしの 32 バイトページ。
  uint16_t offset = page * VIAL_PACKET_SIZE;
  if (offset >= VIAL_KEYBOARD_DEFINITION_SIZE) {
    return;
  }

  uint16_t remaining = VIAL_KEYBOARD_DEFINITION_SIZE - offset;
  uint16_t length = remaining < VIAL_PACKET_SIZE ? remaining : VIAL_PACKET_SIZE;
  memcpy(response, &vial_keyboard_definition[offset], length);
}

/**
 * @brief
 * Dynamic Keymap Buffer のキーインデックスからキーコードを取得する。
 * @param key_index キーインデックス
 * @return キーコード
 */
static uint16_t _keymap_buffer_get_keycode(uint16_t key_index) {
  if (key_index >= KEYMAP_BUFFER_SIZE / 2) {
    return 0;
  }

  // バッファは layer -> row -> column の順で配置される。
  uint8_t layer = (uint8_t)(key_index / (ROWS * COLS));
  uint16_t matrix_index = key_index % (ROWS * COLS);
  uint8_t row = (uint8_t)(matrix_index / COLS);
  uint8_t col = (uint8_t)(matrix_index % COLS);

  icode_t internal = settings_get_keycode(layer, row, col);
  uint16_t vial;
  if (code_convert_to_vial(internal, &vial)) {
    return vial;
  }
  return 0;
}

/**
 * @brief キーマップバッファから指定されたオフセットのバイトを取得する。
 * @param offset バイトオフセット
 * @return 指定されたオフセットのバイト
 */
static uint8_t _keymap_buffer_get_byte(uint16_t offset) {
  if (offset >= KEYMAP_BUFFER_SIZE) {
    return 0;
  }

  uint16_t key_index = offset / 2;
  uint16_t keycode = _keymap_buffer_get_keycode(key_index);
  // VIA の Dynamic Keymap Buffer では
  // 16ビットキーコードをビッグエンディアンで送る。
  return offset % 2 == 0 ? (uint8_t)(keycode >> 8) : (uint8_t)keycode;
}

/**
 * @brief 指定されたキーコードが書き込み可能かどうかを判定する。
 * @param keycode 判定するキーコード
 * @return 書き込み可能な場合はtrue、書き込み不可の場合はfalse
 */
static bool _vial_keycode_write_allowed(uint16_t keycode) {
  return unlocked || !code_convert_is_dangerous_vial_code(keycode);
}

/**
 * @brief スイッチマトリクスの状態を応答バッファに書き込む。
 * @param response 応答バッファ
 */
static void _write_switch_matrix_state(uint8_t response[VIAL_PACKET_SIZE]) {
  const uint16_t bytes_per_row = (COLS + 7) / 8;

  for (uint8_t row = 0; row < ROWS; row++) {
    for (uint8_t col = 0; col < COLS; col++) {
      if (!matrix_scan_is_pressed(row, col)) {
        continue;
      }

      uint16_t byte_in_row = col / 8;
      uint16_t response_index =
          2 + row * bytes_per_row + (bytes_per_row - byte_in_row - 1);
      response[response_index] |= (uint8_t)(1u << (col % 8));
    }
  }
}

/**
 * @brief Vialコマンドを処理する。
 * @param request 受信したリクエストパケット
 * @param response 応答パケットのバッファ
 */
static void _handle_vial_command(const uint8_t request[VIAL_PACKET_SIZE],
                                 uint8_t response[VIAL_PACKET_SIZE]) {
  switch (request[1]) {
  case 0x00: {
    // Keyboard ID / Protocol Query
    // 固有 UID と対応プロトコルを返す。
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    _write_u32_le(response, VIAL_PROTOCOL_VERSION);
    memcpy(&response[4], board_id.id, sizeof(board_id.id));
    break;
  }

  case 0x01:
    // Keyboard Definition Size Query
    _write_u32_le(response, VIAL_KEYBOARD_DEFINITION_SIZE);
    break;

  case 0x02:
    // Keyboard Definition Data Query
    _copy_keyboard_definition_page(_read_u16_le(&request[2]), response);
    break;

  case 0x03: // Get Encoder Value
  case 0x04: // Set Encoder Value
    // スタブ: 未対応。
    response[0] = 1;
    break;

  case 0x05: {
    // Unlock Status Query
    // ロック状態とロック解除キーを確認する。
    response[0] = unlocked ? 1 : 0;
    response[1] = unlock_in_progress ? 1 : 0;
    memcpy(&response[2], vial_unlock_combo, sizeof(vial_unlock_combo));
    break;
  }

  case 0x06:
    // Unlock Start
    // 実際のタイマー開始は最初の解除コンボ検出時に行う。
    unlock_in_progress = true;
    unlock_combo_held = false;
    break;

  case 0x07: {
    // Unlock Poll
    // ポーリング時に解除コンボの保持時間を更新する。
    uint8_t remaining = _vial_update_unlock_state();
    response[0] = unlocked ? 1 : 0;
    response[1] = unlock_in_progress ? 1 : 0;
    response[2] = remaining;
    break;
  }

  case 0x08:
    // Lock
    unlocked = false;
    unlock_in_progress = false;
    unlock_combo_held = false;
    break;

  case 0x09:
    // スタブ: QMK Settings Query は未対応であることを 0xFF 埋めで示す。
    memset(response, 0xFF, VIAL_PACKET_SIZE);
    break;

  case 0x0A: // Get QMK Settings
  case 0x0B: // Set QMK Settings
  case 0x0C: // Reset QMK Settings
    // スタブ: QMK Settings の取得・保存・リセットは未実装のため失敗を返す。
    response[0] = 1;
    break;

  case 0x0D: // Dynamic Entry Operation
    // スタブ: Dynamic Entry は全て未実装。ゼロ初期化済みの空応答を返す。
    // 未実装の機能:
    // - tab dance
    // - combo
    // - key override
    // - alt repeat key
    // - caps word
    // - layer lock
    break;

  default:
    // スタブ: 未定義の Vial コマンドは失敗として扱う
    response[0] = 1;
    break;
  }
}

/**
 * @brief Dynamic KeymapのGETコマンドを処理する。
 * @param request 受信したリクエストパケット
 * @param response 応答パケットのバッファ
 */
static void _handle_dynamic_keymap_get(const uint8_t request[VIAL_PACKET_SIZE],
                                       uint8_t response[VIAL_PACKET_SIZE]) {
  // ホストは byte 0..3 をリクエストのエコー、byte 4.. を実データとして扱う。
  memcpy(response, request, VIAL_KEYMAP_RESPONSE_HEADER_SIZE);
  uint16_t offset = _read_u16_be(&request[1]);

  // パケットが規定の長さを超えないように調整する。
  uint8_t size;
  if (request[3] > VIAL_PACKET_SIZE - VIAL_KEYMAP_RESPONSE_HEADER_SIZE) {
    size = VIAL_PACKET_SIZE - VIAL_KEYMAP_RESPONSE_HEADER_SIZE;
  } else {
    size = request[3];
  }

  for (uint8_t index = 0; index < size; index++) {
    response[VIAL_KEYMAP_RESPONSE_HEADER_SIZE + index] =
        _keymap_buffer_get_byte(offset + index);
  }
}

/**
 * @brief VIAコマンドを処理する。
 * @param request 受信したリクエストパケット
 * @param response 応答パケットのバッファ
 */
static void _handle_via_command(const uint8_t request[VIAL_PACKET_SIZE],
                                uint8_t response[VIAL_PACKET_SIZE]) {
  switch (request[0]) {
  case 0x01:
    // Get Protocol Version
    response[0] = request[0];
    _write_u16_be(&response[1], VIA_PROTOCOL_VERSION);
    break;

  case 0x02:
    // Get Keyboard Value
    // スタブ: マトリクス状態取得以外は未実装
    response[0] = request[0];
    response[1] = request[1];
    if (request[1] == VIA_VALUE_ID_SWITCH_MATRIX_STATE) {
      if (!unlocked) {
        response[0] = 1;
        break;
      }
      _write_switch_matrix_state(response);
    }
    break;

  case 0x03: // Set Keyboard Value
    // スタブ: 未実装。
    response[0] = 1;
    break;

  case 0x04: {
    // Get Keycode
    memcpy(response, request, 3);
    icode_t internal = settings_get_keycode(request[1], request[2], request[3]);
    uint16_t vial;
    if (code_convert_to_vial(internal, &vial)) {
      _write_u16_be(&response[3], vial);
    } else {
      _write_u16_be(&response[3], 0);
    }
    break;
  }

  case 0x05: {
    // Set Keycode
    response[0] = 1;

    uint16_t vial = _read_u16_be(&request[4]);
    if (!_vial_keycode_write_allowed(vial)) {
      break;
    }

    icode_t internal;
    if (!code_convert_to_internal(vial, &internal)) {
      break;
    }

    if (settings_set_keycode(request[1], request[2], request[3], internal) !=
        SETTINGS_UPDATE_PERSISTED) {
      break;
    }

    response[0] = 0;
    break;
  }

  case 0x06:
    // Dynamic Keymap Reset
    if (unlocked &&
        settings_reset(SETTINGS_ID_KEYMAP) == SETTINGS_UPDATE_PERSISTED) {
      response[0] = 0;
    } else {
      response[0] = 1;
    }
    break;

  case 0x07: // Set Lighting Value
  case 0x08: // Get Lighting Value
  case 0x09: // Save Lighting Value
    // スタブ: Lighting / VialRGB の取得・保存・設定は未実装。
    response[0] = 1;
    break;

  case 0x0B:
    // Bootloader Jump
    if (unlocked) {
      state_set_system(STATE_BOOTLOADER);
    } else {
      response[0] = 1;
    }
    break;

  case 0x0C:
    // Get Macro Count
    // スタブ: マクロは未実装のため、Macro Count は 0 を返す。
    response[0] = request[0];
    break;

  case 0x0D:
    // Get Macro Buffer Size
    // スタブ: マクロバッファは未実装のため、Macro Buffer Size は 0 を返す。
    response[0] = request[0];
    break;

  case 0x0E:
    // Get Macro Buffer
    // スタブ: Macro Buffer Get は要求ヘッダだけをエコーし、データはゼロで返す。
    memcpy(response, request, 4);
    break;

  case 0x0F: // Set Macro Buffer
    // スタブ: マクロの設定は未実装。
    response[0] = 1;
    break;

  case 0x11:
    // Get Layer Count
    response[0] = request[0];
    response[1] = KEYMAP_LAYER_COUNT;
    break;

  case 0x12:
    // Dynamic Keymap Buffer Get
    _handle_dynamic_keymap_get(request, response);
    break;

  default:
    // スタブ: 未定義の VIA コマンドは失敗として扱う。
    response[0] = 1;
    break;
  }
}

#endif // PWMK_ENABLE_USB
