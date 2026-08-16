#include "keyboard/code_convert.h"
#include "keyboard/code.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 表による変換において対応する値がないことを示す値
#define KEYCODE_NOT_FOUND 0x00

// VIAL側に対応する値が存在しないPWMKの内部キーコードであることを
// VIAL側に伝えるための値。
// 変換失敗することと、変換不可能であることを区別する。
#define FOR_VIAL_KEYCODE_PWMK_ONLY 0x00

// PWMK内部とVIAL側のキーコードの対応
typedef struct {
  icode_t internal; // PWMK内部のキーコード
  uint16_t vial;    // VIAL側のキーコード
} code_conversion_t;

enum {
  VIAL_KEYCODE_MUTE = 0x00A8,
  VIAL_KEYCODE_VOLUME_UP = 0x00A9,
  VIAL_KEYCODE_VOLUME_DOWN = 0x00AA,
  VIAL_KEYCODE_MEDIA_NEXT_TRACK = 0x00AB,
  VIAL_KEYCODE_MEDIA_PREV_TRACK = 0x00AC,
  VIAL_KEYCODE_MEDIA_STOP = 0x00AD,
  VIAL_KEYCODE_MEDIA_PLAY_PAUSE = 0x00AE,
  VIAL_KEYCODE_MEDIA_EJECT = 0x00B0,
  VIAL_KEYCODE_MEDIA_FAST_FORWARD = 0x00BB,
  VIAL_KEYCODE_MEDIA_REWIND = 0x00BC,
  VIAL_KEYCODE_MOUSE_UP = 0x00CD,
  VIAL_KEYCODE_MOUSE_DOWN = 0x00CE,
  VIAL_KEYCODE_MOUSE_LEFT = 0x00CF,
  VIAL_KEYCODE_MOUSE_RIGHT = 0x00D0,
  VIAL_KEYCODE_MOUSE_BUTTON_1 = 0x00D1,
  VIAL_KEYCODE_MOUSE_BUTTON_2 = 0x00D2,
  VIAL_KEYCODE_MOUSE_BUTTON_3 = 0x00D3,
  VIAL_KEYCODE_MOUSE_BUTTON_4 = 0x00D4,
  VIAL_KEYCODE_MOUSE_BUTTON_5 = 0x00D5,
  VIAL_KEYCODE_MOUSE_BUTTON_6 = 0x00D6,
  VIAL_KEYCODE_MOUSE_BUTTON_7 = 0x00D7,
  VIAL_KEYCODE_MOUSE_BUTTON_8 = 0x00D8,
  VIAL_KEYCODE_MOUSE_WHEEL_UP = 0x00D9,
  VIAL_KEYCODE_MOUSE_WHEEL_DOWN = 0x00DA,
  VIAL_KEYCODE_MOUSE_WHEEL_LEFT = 0x00DB,
  VIAL_KEYCODE_MOUSE_WHEEL_RIGHT = 0x00DC,
  VIAL_KEYCODE_CONNECTION_NEXT = 0x7781,
  VIAL_KEYCODE_CONNECTION_AUTO = 0x7780,
  VIAL_KEYCODE_CONNECTION_BLE = 0x7786,
  VIAL_KEYCODE_BLE_UNPAIR = 0x7792,
  VIAL_KEYCODE_BLE_SLOT_1 = 0x7793,
  VIAL_KEYCODE_BLE_SLOT_2 = 0x7794,
  VIAL_KEYCODE_BLE_SLOT_3 = 0x7795,
  VIAL_KEYCODE_BLE_SLOT_4 = 0x7796,
  VIAL_KEYCODE_BLE_SLOT_5 = 0x7797,
  VIAL_KEYCODE_USER_0 = 0x7E40,
  VIAL_KEYCODE_USER_31 = 0x7E5F,
};

#define VIAL_KEYCODE_USER_COUNT (VIAL_KEYCODE_USER_31 - VIAL_KEYCODE_USER_0 + 1)

static const code_conversion_t code_conversion_table[] = {
    {ICC_RECORD, KEYCODE_NOT_FOUND},
    {ICC_FAST_FORWARD, VIAL_KEYCODE_MEDIA_FAST_FORWARD},
    {ICC_REWIND, VIAL_KEYCODE_MEDIA_REWIND},
    {ICC_NEXT_TRACK, VIAL_KEYCODE_MEDIA_NEXT_TRACK},
    {ICC_PREV_TRACK, VIAL_KEYCODE_MEDIA_PREV_TRACK},
    {ICC_STOP_TRACK, VIAL_KEYCODE_MEDIA_STOP},
    {ICC_EJECT, VIAL_KEYCODE_MEDIA_EJECT},
    {ICC_RANDOM_PLAY, KEYCODE_NOT_FOUND},
    {ICC_STOP_EJECT, KEYCODE_NOT_FOUND},
    {ICC_PLAY_PAUSE, VIAL_KEYCODE_MEDIA_PLAY_PAUSE},
    {ICC_VOL_MUTE, VIAL_KEYCODE_MUTE},
    {ICC_VOL_UP, VIAL_KEYCODE_VOLUME_UP},
    {ICC_VOL_DOWN, VIAL_KEYCODE_VOLUME_DOWN},
    {IMC_MOUSE_LEFT, VIAL_KEYCODE_MOUSE_BUTTON_1},
    {IMC_MOUSE_RIGHT, VIAL_KEYCODE_MOUSE_BUTTON_2},
    {IMC_MOUSE_MIDDLE, VIAL_KEYCODE_MOUSE_BUTTON_3},
    {KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_4},
    {KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_5},
    {KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_6},
    {KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_7},
    {KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_BUTTON_8},
    {IMC_MOUSE_MOVE_UP, VIAL_KEYCODE_MOUSE_UP},
    {IMC_MOUSE_MOVE_DOWN, VIAL_KEYCODE_MOUSE_DOWN},
    {IMC_MOUSE_MOVE_LEFT, VIAL_KEYCODE_MOUSE_LEFT},
    {IMC_MOUSE_MOVE_RIGHT, VIAL_KEYCODE_MOUSE_RIGHT},
    {IMC_MOUSE_WHEEL_UP, VIAL_KEYCODE_MOUSE_WHEEL_UP},
    {IMC_MOUSE_WHEEL_DOWN, VIAL_KEYCODE_MOUSE_WHEEL_DOWN},
    {KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_WHEEL_LEFT},
    {KEYCODE_NOT_FOUND, VIAL_KEYCODE_MOUSE_WHEEL_RIGHT},
    {ISC_BOOT, VIAL_KEYCODE_BOOTLOADER},
    {ISC_CONN_TOGGLE, VIAL_KEYCODE_CONNECTION_NEXT},
    {ISC_CONN_USB, VIAL_KEYCODE_CONNECTION_AUTO},
    {ISC_CONN_BLE, VIAL_KEYCODE_CONNECTION_BLE},
    {ISC_BLE_UNPAIR, VIAL_KEYCODE_BLE_UNPAIR},
    {ISC_BLE_SLOT_1, VIAL_KEYCODE_BLE_SLOT_1},
    {ISC_BLE_SLOT_2, VIAL_KEYCODE_BLE_SLOT_2},
    {ISC_BLE_SLOT_3, VIAL_KEYCODE_BLE_SLOT_3},
    {ISC_BLE_SLOT_4, VIAL_KEYCODE_BLE_SLOT_4},
    {KEYCODE_NOT_FOUND, VIAL_KEYCODE_BLE_SLOT_5},
};

/*
 * 内部関数宣言
 */

static bool _code_convert_standard_to_vial(icode_t keycode_internal,
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
  if (_code_convert_mapped_to_vial(keycode_internal, keycode_vial)) {
    return true;
  }
  if (_code_convert_user_to_vial(keycode_internal, keycode_vial)) {
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
static bool _code_convert_internal_base_to_vial(uint8_t keycode_internal,
                                                uint8_t *keycode_vial) {
  // 0x00はHID標準ではreserved領域であり、PWMKとVIALはNOOPとして扱う。
  // 0x01から0x03の範囲はHID標準で特殊な扱いになっている。
  if (IKC_NOOP < keycode_internal && keycode_internal < IKC_A) {
    return false;
  }
  // この範囲はHID標準ではキーパッド系のキーが割り当てられているが、
  // VIALではこの範囲を扱えない。
  if (IKC_EXSEL < keycode_internal && keycode_internal < IMKC_LEFT_CONTROL) {
    *keycode_vial = FOR_VIAL_KEYCODE_PWMK_ONLY;
    return true;
  }
  // 右GUIキー(0x00E7)以降はHID標準ではreserved領域。
  if (IMKC_RIGHT_GUI < keycode_internal) {
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
  if (IKC_NOOP < keycode_vial && keycode_vial < IKC_A) {
    return false;
  }

  // VIALのこの範囲のキーコードは変換表で変換するので、ここでは変換できないとする。
  if (IKC_EXSEL < keycode_vial && keycode_vial < IMKC_LEFT_CONTROL) {
    return false;
  }

  // VIALのこの範囲のキーコードは変換表またはビット解析などで変換するので、ここでは変換できないとする。
  if (IMKC_RIGHT_GUI < keycode_vial) {
    return false;
  }

  *keycode_internal = (icode_t)keycode_vial;
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

  uint8_t left_modifiers = modifiers_internal & 0x0F;
  uint8_t right_modifiers = modifiers_internal & 0xF0;

  if (left_modifiers != 0 && right_modifiers != 0) {
    return false;
  }

  if (right_modifiers != 0) {
    *modifiers_vial = 0x10 | (right_modifiers >> 4);
    return true;
  }

  *modifiers_vial = left_modifiers;
  return true;
}

/**
 * @brief VIALの修飾ビット表現を内部用の修飾ビット表現に変換する。
 * @param modifiers_vial 変換するVIALの修飾ビット表現
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static uint8_t
_code_convert_vial_modifiers_to_internal(uint8_t modifiers_vial) {
  uint8_t modifiers = modifiers_vial & 0x0F;
  return (modifiers_vial & 0x10) != 0 ? modifiers << 4 : modifiers;
}

/**
 * @brief 内部用の標準キーコードをVIALのキーコードに変換する。
 * @param keycode_internal 変換する内部用のキーコード
 * @param keycode_vial 変換後のVIALのキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_standard_to_vial(icode_t keycode_internal,
                                           uint16_t *keycode_vial) {
  uint32_t keycode = (uint32_t)keycode_internal;

  // 上位16ビットが0でない場合は、標準キーコードではないためここでは変換できないとする。
  if ((keycode & 0xFFFF0000U) != 0) {
    return false;
  }

  // 下位8ビットのベース部分をVIALのキーコードに変換する。
  // 変換に失敗する場合は、標準キーコードではないためここでは変換できないとする。
  uint8_t base_internal = (uint8_t)keycode;
  uint8_t base_vial;
  if (!_code_convert_internal_base_to_vial(base_internal, &base_vial)) {
    return false;
  }

  // 0x0000FF00の部分が修飾となる。
  // 修飾がない場合は、変換したベース部分をVIALのキーコードとして返す。
  uint8_t modifiers_internal = (uint8_t)(keycode >> 8);
  if (modifiers_internal == 0) {
    *keycode_vial = base_vial;
    return true;
  }

  // 修飾キーコードをVIALの修飾キーコードに変換する。
  // 変換に失敗する場合は、標準キーコードではないためここでは変換できないとする。
  uint8_t modifiers_vial;
  if (!_code_convert_internal_modifiers_to_vial(modifiers_internal,
                                                &modifiers_vial)) {
    return false;
  }

  // 修飾キーコードとベース部分を組み合わせてVIALのキーコードとして返す。
  *keycode_vial = ((uint16_t)modifiers_vial << 8) | base_vial;
  return true;
}

/**
 * @brief VIALの標準キーコードを内部用のキーコードに変換する。
 * @param keycode_vial 変換するVIALのキーコード
 * @param keycode_internal 変換後の内部用のキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
static bool _code_convert_standard_to_internal(uint16_t keycode_vial,
                                               icode_t *keycode_internal) {
  uint8_t modifiers_vial = (uint8_t)(keycode_vial >> 8);
  if (modifiers_vial > 0x1F || modifiers_vial == 0x10) {
    return false;
  }

  uint8_t base_vial = (uint8_t)keycode_vial;
  icode_t base_internal;
  if (!_code_convert_vial_base_to_internal(base_vial, &base_internal)) {
    return false;
  }

  *keycode_internal =
      ((icode_t)_code_convert_vial_modifiers_to_internal(modifiers_vial) << 8) |
      base_internal;
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
  if (keycode_internal == KEYCODE_NOT_FOUND) {
    return false;
  }

  for (size_t index = 0;
       index < sizeof(code_conversion_table) / sizeof(code_conversion_table[0]);
       index++) {
    if (code_conversion_table[index].internal == keycode_internal) {
      if (code_conversion_table[index].vial == KEYCODE_NOT_FOUND) {
        *keycode_vial = FOR_VIAL_KEYCODE_PWMK_ONLY;
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
  if (keycode_vial == KEYCODE_NOT_FOUND) {
    return false;
  }

  for (size_t index = 0;
       index < sizeof(code_conversion_table) / sizeof(code_conversion_table[0]);
       index++) {
    if (code_conversion_table[index].vial == keycode_vial) {
      if (code_conversion_table[index].internal == KEYCODE_NOT_FOUND) {
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
  if (keycode_internal < IUC_RANGE_MIN || keycode_internal > IUC_RANGE_MAX) {
    return false;
  }

  const uint16_t index = keycode_internal - IUC_RANGE_MIN;
  if (index >= VIAL_KEYCODE_USER_COUNT) {
    *keycode_vial = FOR_VIAL_KEYCODE_PWMK_ONLY;
    return true;
  }

  *keycode_vial = VIAL_KEYCODE_USER_0 + index;
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
  if (keycode_vial < VIAL_KEYCODE_USER_0 ||
      keycode_vial > VIAL_KEYCODE_USER_31) {
    return false;
  }

  const uint16_t index = keycode_vial - VIAL_KEYCODE_USER_0;
  *keycode_internal = IUC_RANGE_MIN + index;
  return true;
}
