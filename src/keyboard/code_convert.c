#include "keyboard/code_convert.h"
#include "keyboard/code.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 0x00は未設定または無効なキーコードとして扱う。
#define CODE_CONVERT_NONE 0x0000
// VIALが扱える標準キーコードの範囲の開始
#define CODE_CONVERT_HID_KEYCODE_FIRST 0x0004
// VIALが扱える標準キーコードの範囲の終了
#define CODE_CONVERT_HID_KEYCODE_EXSEL 0x00A4
// 標準外キーパッド系の範囲の開始
#define CODE_CONVERT_HID_KEYPAD_FIRST 0x00B0
// 標準外キーパッド系の範囲の終了
#define CODE_CONVERT_HID_KEYPAD_LAST 0x00DD
// 修飾キーの範囲の開始
#define CODE_CONVERT_HID_MODIFIER_FIRST 0x00E0
// 修飾キーの範囲の終了
#define CODE_CONVERT_HID_MODIFIER_LAST 0x00E7
// 内部修飾ビット列の左手側マスク
#define CODE_CONVERT_INTERNAL_LEFT_MOD_MASK 0x0F
// 内部修飾ビット列の右手側マスク
#define CODE_CONVERT_INTERNAL_RIGHT_MOD_MASK 0xF0
// VIAL修飾ビット列のマスク
#define CODE_CONVERT_VIAL_MOD_MASK 0x0F
// VIAL修飾ビット列の右手側フラグ
#define CODE_CONVERT_VIAL_RIGHT_MODIFIER_FLAG 0x10
// VIAL修飾ビット列の最大値
#define CODE_CONVERT_VIAL_MODIFIER_MAX 0x1F
// VIAL修飾ビット列の左右指定用シフト量
#define CODE_CONVERT_VIAL_MODIFIER_LR_SHIFT 4
// VIAL修飾ビット列のシフト量
#define CODE_CONVERT_VIAL_MODIFIER_SHIFT 8
// 変換に対応するキーコードが見つからなかった場合の定義
#define CODE_CONVERT_KEYCODE_NOT_FOUND CODE_CONVERT_NONE
// VIAL側でPWMK専用のキーコードとして扱う場合の定義
#define CODE_CONVERT_VIAL_KEYCODE_PWMK_ONLY CODE_CONVERT_NONE

// PWMK内部とVIAL側のキーコードの対応
typedef struct {
  icode_t internal; // PWMK内部のキーコード
  uint16_t vial;    // VIAL側のキーコード
} code_conversion_t;

static const code_conversion_t code_conversion_table[] = {
    {ICC_RECORD, CODE_CONVERT_KEYCODE_NOT_FOUND},
    {ICC_FAST_FORWARD, VIAL_KEYCODE_MEDIA_FAST_FORWARD},
    {ICC_REWIND, VIAL_KEYCODE_MEDIA_REWIND},
    {ICC_NEXT_TRACK, VIAL_KEYCODE_MEDIA_NEXT_TRACK},
    {ICC_PREV_TRACK, VIAL_KEYCODE_MEDIA_PREV_TRACK},
    {ICC_STOP_TRACK, VIAL_KEYCODE_MEDIA_STOP},
    {ICC_EJECT, VIAL_KEYCODE_MEDIA_EJECT},
    {ICC_RANDOM_PLAY, CODE_CONVERT_KEYCODE_NOT_FOUND},
    {ICC_STOP_EJECT, CODE_CONVERT_KEYCODE_NOT_FOUND},
    {ICC_PLAY_PAUSE, VIAL_KEYCODE_MEDIA_PLAY_PAUSE},
    {ICC_VOL_MUTE, VIAL_KEYCODE_MUTE},
    {ICC_VOL_UP, VIAL_KEYCODE_VOLUME_UP},
    {ICC_VOL_DOWN, VIAL_KEYCODE_VOLUME_DOWN},
    {IMC_MOUSE_LEFT, VIAL_KEYCODE_MOUSE_BUTTON_1},
    {IMC_MOUSE_RIGHT, VIAL_KEYCODE_MOUSE_BUTTON_2},
    {IMC_MOUSE_MIDDLE, VIAL_KEYCODE_MOUSE_BUTTON_3},
    {CODE_CONVERT_KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_4},
    {CODE_CONVERT_KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_5},
    {CODE_CONVERT_KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_6},
    {CODE_CONVERT_KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_7},
    {CODE_CONVERT_KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_8},
    {IMC_MOUSE_MOVE_UP, VIAL_KEYCODE_MOUSE_UP},
    {IMC_MOUSE_MOVE_DOWN, VIAL_KEYCODE_MOUSE_DOWN},
    {IMC_MOUSE_MOVE_LEFT, VIAL_KEYCODE_MOUSE_LEFT},
    {IMC_MOUSE_MOVE_RIGHT, VIAL_KEYCODE_MOUSE_RIGHT},
    {IMC_MOUSE_WHEEL_UP, VIAL_KEYCODE_MOUSE_WHEEL_UP},
    {IMC_MOUSE_WHEEL_DOWN, VIAL_KEYCODE_MOUSE_WHEEL_DOWN},
    {CODE_CONVERT_KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_WHEEL_LEFT},
    {CODE_CONVERT_KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_WHEEL_RIGHT},
    {ISC_BOOT, VIAL_KEYCODE_BOOTLOADER},
    {ISC_CONN_TOGGLE, VIAL_KEYCODE_CONNECTION_NEXT},
    {ISC_CONN_USB, VIAL_KEYCODE_CONNECTION_AUTO},
    {ISC_CONN_BLE, VIAL_KEYCODE_CONNECTION_BLE},
    {ISC_BLE_UNPAIR, VIAL_KEYCODE_BLE_UNPAIR},
    {ISC_BLE_SLOT_1, VIAL_KEYCODE_BLE_SLOT_1},
    {ISC_BLE_SLOT_2, VIAL_KEYCODE_BLE_SLOT_2},
    {ISC_BLE_SLOT_3, VIAL_KEYCODE_BLE_SLOT_3},
    {ISC_BLE_SLOT_4, VIAL_KEYCODE_BLE_SLOT_4},
    {CODE_CONVERT_KEYCODE_NOT_FOUND, VIAL_KEYCODE_BLE_SLOT_5},
};

/*
 * 内部関数宣言
 */

static bool _code_convert_standard_to_vial(icode_t keycode_internal,
                                           uint16_t *keycode_vial);
static bool _code_convert_modifier_to_vial(icode_t keycode_internal,
                                           uint16_t *keycode_vial);
static bool _code_convert_standard_to_internal(uint16_t keycode_vial,
                                               icode_t *keycode_internal);
static bool _code_convert_mapped_to_vial(icode_t keycode_internal,
                                         uint16_t *keycode_vial);
static bool _code_convert_mapped_to_internal(uint16_t keycode_vial,
                                             icode_t *keycode_internal);
static bool _code_convert_user_to_vial(icode_t keycode_internal,
                                       uint16_t *keycode_vial);
static bool _code_convert_user_to_internal(uint16_t keycode_vial,
                                           icode_t *keycode_internal);
static bool _code_convert_internal_modifiers_to_vial_keycode(
    uint8_t modifiers_internal, uint8_t base_vial, uint16_t *keycode_vial);
static bool
_code_convert_vial_keycode_modifiers_to_internal(uint16_t keycode_vial,
                                                 uint8_t *modifiers_internal);

/*
 * 公開関数
 */

/**
 * @brief 内部用キーコードをVIALのキーコードに変換する。
 * @param keycode_internal 変換する内部用キーコード
 * @param keycode_vial 変換後のVIALのキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
bool code_convert_to_vial(icode_t keycode_internal, uint16_t *keycode_vial) {
  if (keycode_internal == CODE_CONVERT_NONE) {
    *keycode_vial = CODE_CONVERT_NONE;
    return true;
  }

  if (_code_convert_mapped_to_vial(keycode_internal, keycode_vial)) {
    return true;
  }
  if (_code_convert_user_to_vial(keycode_internal, keycode_vial)) {
    return true;
  }
  if (_code_convert_modifier_to_vial(keycode_internal, keycode_vial)) {
    return true;
  }
  if (_code_convert_standard_to_vial(keycode_internal, keycode_vial)) {
    return true;
  }
  return false;
}

/**
 * @brief VIALのキーコードを内部用キーコードに変換する。
 * @param keycode_vial 変換するVIALのキーコード
 * @param keycode_internal 変換後の内部用のキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
bool code_convert_to_internal(uint16_t keycode_vial,
                              icode_t *keycode_internal) {
  if (keycode_vial == CODE_CONVERT_NONE) {
    *keycode_internal = CODE_CONVERT_NONE;
    return true;
  }

  if (_code_convert_mapped_to_internal(keycode_vial, keycode_internal)) {
    return true;
  }
  if (_code_convert_user_to_internal(keycode_vial, keycode_internal)) {
    return true;
  }
  if (_code_convert_standard_to_internal(keycode_vial, keycode_internal)) {
    return true;
  }
  return false;
}

/**
 * @brief
 * 指定したVIALのキーコードがブートローダー起動用のキーコードかどうかを判定する。
 * @param keycode_vial VIALのキーコード
 * @return ブートローダー起動用のキーコードの場合はtrue、そうでない場合はfalse
 */
bool code_convert_is_dangerous_vial_code(uint16_t keycode_vial) {
  return keycode_vial == VIAL_KEYCODE_BOOTLOADER;
}

/*
 * 内部関数
 */

/**
 * @brief 内部用のキーコードの下位8ビット部分をVIALのキーコードに変換する。
 *        基本的に下位8ビット部分はPWMKとVIALで同じ値を持つが、
 *        部分的に対応していないキーコードがあるため、そのチェックを行う。
 * @param keycode_internal 変換する内部用のキーコードの下位8ビット部分
 * @param keycode_vial 変換後のVIALのキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_internal_base_to_vial(uint16_t keycode_internal,
                                                uint8_t *keycode_vial) {
  // 0x00はHID標準ではreserved領域であり、PWMKとVIALはNOOPとして扱う。
  // 0x01から0x03の範囲はHID標準で特殊な扱いになっている。
  if (keycode_internal > CODE_CONVERT_NONE &&
      keycode_internal < CODE_CONVERT_HID_KEYCODE_FIRST) {
    return false;
  }
  // この範囲はHID標準ではキーパッド系のキーが割り当てられているが、
  // VIALではこの範囲を扱えない。
  if (keycode_internal > CODE_CONVERT_HID_KEYCODE_EXSEL &&
      keycode_internal < CODE_CONVERT_HID_MODIFIER_FIRST) {
    *keycode_vial = CODE_CONVERT_VIAL_KEYCODE_PWMK_ONLY;
    return true;
  }
  // 修飾キーのUsage (0xE0..0xE7) はMODIFIER opcodeで表現する。
  if (keycode_internal > CODE_CONVERT_HID_KEYPAD_LAST) {
    return false;
  }

  *keycode_vial = keycode_internal;
  return true;
}

/**
 * @brief VIALのキーコードの下位8ビット部分を内部用のキーコードに変換する。
 * @param keycode_vial 変換するVIALのキーコードの下位8ビット
 * @param keycode_internal 変換後の内部用のキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_vial_base_to_internal(uint8_t keycode_vial,
                                                icode_t *keycode_internal) {
  // 将来的にはIKC_NOOP+1(0x01)はTRANSPARENTになる
  if (keycode_vial > CODE_CONVERT_NONE &&
      keycode_vial < CODE_CONVERT_HID_KEYCODE_FIRST) {
    return false;
  }

  // VIALのこの範囲のキーコードは変換表で変換するので、ここでは変換できないとする。
  if (keycode_vial > CODE_CONVERT_HID_KEYCODE_EXSEL &&
      keycode_vial < CODE_CONVERT_HID_MODIFIER_FIRST) {
    return false;
  }

  // 修飾キーのVIALキーコードが直接指定されている場合は、ここで変換する。
  switch (keycode_vial) {
  case VIAL_KEYCODE_LEFT_CONTROL:
    *keycode_internal = IMKC_LEFT_CONTROL;
    return true;
  case VIAL_KEYCODE_LEFT_SHIFT:
    *keycode_internal = IMKC_LEFT_SHIFT;
    return true;
  case VIAL_KEYCODE_LEFT_ALT:
    *keycode_internal = IMKC_LEFT_ALT;
    return true;
  case VIAL_KEYCODE_LEFT_GUI:
    *keycode_internal = IMKC_LEFT_GUI;
    return true;
  case VIAL_KEYCODE_RIGHT_CONTROL:
    *keycode_internal = IMKC_RIGHT_CONTROL;
    return true;
  case VIAL_KEYCODE_RIGHT_SHIFT:
    *keycode_internal = IMKC_RIGHT_SHIFT;
    return true;
  case VIAL_KEYCODE_RIGHT_ALT:
    *keycode_internal = IMKC_RIGHT_ALT;
    return true;
  case VIAL_KEYCODE_RIGHT_GUI:
    *keycode_internal = IMKC_RIGHT_GUI;
    return true;
  default:
    break;
  }

  // VIALのこの範囲のキーコードは変換表またはビット解析などで変換するので、ここでは変換できないとする。
  if (keycode_vial > CODE_CONVERT_HID_MODIFIER_LAST) {
    return false;
  }

  // 0x00は0x00に変換する。
  if (keycode_vial == CODE_CONVERT_NONE) {
    *keycode_internal = IKC_NOOP;
    return true;
  }

  *keycode_internal =
      ICODE_PACK(ICODE_OPCODE_KEYBOARD, CODE_CONVERT_NONE, keycode_vial);
  return true;
}

/**
 * @brief 内部用の修飾ビット表現をVIALの修飾ビット表現に変換する。
 * @param modifiers_internal 変換する内部用の修飾ビット表現
 * @param modifiers_vial 変換後のVIALの修飾ビット表現を格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_internal_modifiers_to_vial(uint8_t modifiers_internal,
                                                     uint8_t *modifiers_vial) {
  // VIALは左右の修飾キーを完全に重ねない。
  // 0x10の有無で左右の修飾キーを区別する。

  uint8_t left_modifiers =
      modifiers_internal & CODE_CONVERT_INTERNAL_LEFT_MOD_MASK;
  uint8_t right_modifiers =
      modifiers_internal & CODE_CONVERT_INTERNAL_RIGHT_MOD_MASK;

  if (left_modifiers != CODE_CONVERT_NONE &&
      right_modifiers != CODE_CONVERT_NONE) {
    return false;
  }

  // 右手の修飾キーが設定されている場合は、VIALの右手修飾フラグを付与して変換する。
  if (right_modifiers != CODE_CONVERT_NONE) {

    *modifiers_vial = CODE_CONVERT_VIAL_RIGHT_MODIFIER_FLAG |
                      (right_modifiers >> CODE_CONVERT_VIAL_MODIFIER_LR_SHIFT);
    return true;
  }

  *modifiers_vial = left_modifiers;
  return true;
}

/**
 * @brief VIALの修飾ビット表現を内部用の修飾ビット表現に変換する。
 * @param modifiers_vial 変換するVIALの修飾ビット表現
 * @param modifiers_internal 変換後の内部用の修飾ビット表現を格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool
_code_convert_vial_modifiers_to_internal(uint8_t modifiers_vial,
                                         uint8_t *modifiers_internal) {
  // VIALでは修飾ビットを0x00..0x1Fで表し、0x10単独は無効とする。
  if (modifiers_vial > CODE_CONVERT_VIAL_MODIFIER_MAX ||
      modifiers_vial == CODE_CONVERT_VIAL_RIGHT_MODIFIER_FLAG) {
    return false;
  }

  *modifiers_internal = modifiers_vial & CODE_CONVERT_VIAL_MOD_MASK;

  // VIAL側で右手と指定されている場合は、内部表現の右手修飾ビットへ変換する。
  if ((modifiers_vial & CODE_CONVERT_VIAL_RIGHT_MODIFIER_FLAG) !=
      CODE_CONVERT_NONE) {
    *modifiers_internal <<= CODE_CONVERT_VIAL_MODIFIER_LR_SHIFT;
  }
  return true;
}

/**
 * @brief 内部用修飾ビットとVIALベースキーコードをVIALのキーコードに合成する。
 * @param modifiers_internal 内部用の修飾ビット表現
 * @param base_vial VIALのキーコードのベース部分
 * @param keycode_vial 合成後のVIALキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_internal_modifiers_to_vial_keycode(
    uint8_t modifiers_internal, uint8_t base_vial, uint16_t *keycode_vial) {
  if (modifiers_internal == CODE_CONVERT_NONE) {
    *keycode_vial = base_vial;
    return true;
  }

  // 内部用修飾ビットをVIALの修飾ビット表現に変換する。
  uint8_t modifiers_vial;
  if (!_code_convert_internal_modifiers_to_vial(modifiers_internal,
                                                &modifiers_vial)) {
    return false;
  }

  // VIALの修飾ビットとベースキーコードを合成してVIALキーコードを作成する。
  *keycode_vial =
      ((uint16_t)modifiers_vial << CODE_CONVERT_VIAL_MODIFIER_SHIFT) |
      base_vial;
  return true;
}

/**
 * @brief VIALキーコードから修飾ビットを取り出し、内部表現へ変換する。
 * @param keycode_vial 変換するVIALキーコード
 * @param modifiers_internal 変換後の内部用の修飾ビット表現を格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool
_code_convert_vial_keycode_modifiers_to_internal(uint16_t keycode_vial,
                                                 uint8_t *modifiers_internal) {
  // VIALキーコードから修飾ビットを取り出す。
  uint8_t modifiers_vial =
      (uint8_t)(keycode_vial >> CODE_CONVERT_VIAL_MODIFIER_SHIFT);

  // 取り出したVIALの修飾ビットを内部表現に変換する。
  return _code_convert_vial_modifiers_to_internal(modifiers_vial,
                                                  modifiers_internal);
}

/**
 * @brief 内部用の単独修飾キーをVIALの標準キーコードに変換する。
 * @param keycode_internal 変換する内部用の修飾キーコード
 * @param keycode_vial 変換後のVIALキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_modifier_to_vial(icode_t keycode_internal,
                                           uint16_t *keycode_vial) {
  if (ICODE_OPCODE(keycode_internal) != ICODE_OPCODE_MODIFIER ||
      !code_icode_is_valid(keycode_internal)) {
    return false;
  }

  switch (ICODE_MODE(keycode_internal)) {
  case KMC_LEFT_CONTROL:
    *keycode_vial = VIAL_KEYCODE_LEFT_CONTROL;
    return true;
  case KMC_LEFT_SHIFT:
    *keycode_vial = VIAL_KEYCODE_LEFT_SHIFT;
    return true;
  case KMC_LEFT_ALT:
    *keycode_vial = VIAL_KEYCODE_LEFT_ALT;
    return true;
  case KMC_LEFT_GUI:
    *keycode_vial = VIAL_KEYCODE_LEFT_GUI;
    return true;
  case KMC_RIGHT_CONTROL:
    *keycode_vial = VIAL_KEYCODE_RIGHT_CONTROL;
    return true;
  case KMC_RIGHT_SHIFT:
    *keycode_vial = VIAL_KEYCODE_RIGHT_SHIFT;
    return true;
  case KMC_RIGHT_ALT:
    *keycode_vial = VIAL_KEYCODE_RIGHT_ALT;
    return true;
  case KMC_RIGHT_GUI:
    *keycode_vial = VIAL_KEYCODE_RIGHT_GUI;
    return true;
  default:
    return false;
  }
}

/**
 * @brief 内部用の標準キーコードをVIALのキーコードに変換する。
 * @param keycode_internal 変換する内部用のキーコード
 * @param keycode_vial 変換後のVIALのキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_standard_to_vial(icode_t keycode_internal,
                                           uint16_t *keycode_vial) {
  // 標準キーコードで有効であることを確認する。
  if (ICODE_OPCODE(keycode_internal) != ICODE_OPCODE_KEYBOARD ||
      !code_icode_is_valid(keycode_internal)) {
    return false;
  }

  // 内部用のベースキーコードをVIALのベースキーコードに変換する。
  uint16_t base_internal = ICODE_OPERAND(keycode_internal);
  uint8_t base_vial;
  if (!_code_convert_internal_base_to_vial(base_internal, &base_vial)) {
    return false;
  }

  // 内部用の修飾ビット取り出す。
  uint8_t modifiers_internal = ICODE_MODE(keycode_internal);

  // 内部用の修飾ビットとVIALのベースキーコードをVIALのキーコードに合成して返す。
  return _code_convert_internal_modifiers_to_vial_keycode(
      modifiers_internal, base_vial, keycode_vial);
}

/**
 * @brief VIALの標準キーコードを内部用のキーコードに変換する。
 * @param keycode_vial 変換するVIALのキーコード
 * @param keycode_internal 変換後の内部用のキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_standard_to_internal(uint16_t keycode_vial,
                                               icode_t *keycode_internal) {
  // VIALキーコードから修飾ビットを内部表現に変換する。
  uint8_t modifiers_internal;
  if (!_code_convert_vial_keycode_modifiers_to_internal(keycode_vial,
                                                        &modifiers_internal)) {
    return false;
  }

  // VIALキーコードからベースキーコードを取り出し、内部表現に変換する。
  uint8_t base_vial = (uint8_t)keycode_vial;
  icode_t base_internal;
  if (!_code_convert_vial_base_to_internal(base_vial, &base_internal)) {
    return false;
  }

  // VIALの修飾キーがベースキーコードとして指定されている場合の処理。
  if (base_vial >= CODE_CONVERT_HID_MODIFIER_FIRST &&
      base_vial <= CODE_CONVERT_HID_MODIFIER_LAST) {
    if (modifiers_internal != CODE_CONVERT_NONE) {
      // 修飾キーに修飾ビットが設定されている場合は無効とする。
      return false;
    }
    // 変換済みの内部表現がそのまま内部用キーコードとして使用される。
    *keycode_internal = base_internal;
    return true;
  }

  // VIALのベースキーコードが無効の場合の処理。
  if (base_vial == CODE_CONVERT_NONE) {
    if (modifiers_internal != CODE_CONVERT_NONE) {
      // 無効なベースキーコードに修飾ビットが設定されている場合は無効とする。
      return false;
    }
    // 無効なベースキーコードは内部用ではIKC_NOOPとして扱う。
    *keycode_internal = IKC_NOOP;
    return true;
  }

  // 通常のキーコード変換処理。
  *keycode_internal = ICODE_PACK(ICODE_OPCODE_KEYBOARD, modifiers_internal,
                                 ICODE_OPERAND(base_internal));
  return true;
}

/**
 * @brief 変換表にある内部用のキーコードをVIALのキーコードに変換する。
 * @param keycode_internal 変換する内部用のキーコード
 * @param keycode_vial 変換後のVIALのキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_mapped_to_vial(icode_t keycode_internal,
                                         uint16_t *keycode_vial) {
  if (keycode_internal == CODE_CONVERT_KEYCODE_NOT_FOUND) {
    return false;
  }

  const size_t table_size =
      sizeof(code_conversion_table) / sizeof(code_conversion_table[0]);
  for (size_t index = 0; index < table_size; index++) {
    if (code_conversion_table[index].internal == keycode_internal) {
      if (code_conversion_table[index].vial == CODE_CONVERT_KEYCODE_NOT_FOUND) {
        *keycode_vial = CODE_CONVERT_VIAL_KEYCODE_PWMK_ONLY;
        return true;
      }
      *keycode_vial = code_conversion_table[index].vial;
      return true;
    }
  }

  return false;
}

/**
 * @brief 変換表にあるVIALのキーコードを内部用のキーコードに変換する。
 * @param keycode_vial 変換するVIALのキーコード
 * @param keycode_internal 変換後の内部用のキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_mapped_to_internal(uint16_t keycode_vial,
                                             icode_t *keycode_internal) {
  if (keycode_vial == CODE_CONVERT_KEYCODE_NOT_FOUND) {
    return false;
  }

  for (size_t index = 0;
       index < sizeof(code_conversion_table) / sizeof(code_conversion_table[0]);
       index++) {
    if (code_conversion_table[index].vial == keycode_vial) {
      if (code_conversion_table[index].internal ==
          CODE_CONVERT_KEYCODE_NOT_FOUND) {
        return false;
      }
      *keycode_internal = code_conversion_table[index].internal;
      return true;
    }
  }

  return false;
}

/**
 * @brief 内部用のユーザー定義キーコードをVIALのキーコードに変換する。
 * @param keycode_internal 変換する内部用のユーザー定義キーコード
 * @param keycode_vial 変換後のVIALのキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_user_to_vial(icode_t keycode_internal,
                                       uint16_t *keycode_vial) {
  const uint8_t opcode = ICODE_OPCODE(keycode_internal);
  if (opcode < ICODE_OPCODE_USER_MIN || opcode > ICODE_OPCODE_USER_MAX) {
    return false;
  }

  const uint32_t index = (uint32_t)keycode_internal - IUC_RANGE_MIN;
  if (index >= VIAL_KEYCODE_USER_COUNT) {
    *keycode_vial = CODE_CONVERT_VIAL_KEYCODE_PWMK_ONLY;
    return true;
  }

  *keycode_vial = VIAL_KEYCODE_USER_START + index;
  return true;
}

/**
 * @brief VIALのユーザー定義キーコードを内部用のキーコードに変換する。
 * @param keycode_vial 変換するVIALのユーザー定義キーコード
 * @param keycode_internal 変換後の内部用のキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_user_to_internal(uint16_t keycode_vial,
                                           icode_t *keycode_internal) {
  if (keycode_vial < VIAL_KEYCODE_USER_START ||
      keycode_vial > VIAL_KEYCODE_USER_END) {
    return false;
  }

  const uint16_t index = keycode_vial - VIAL_KEYCODE_USER_START;
  *keycode_internal =
      ICODE_PACK(ICODE_OPCODE_USER_MIN, CODE_CONVERT_NONE, index);
  return true;
}
