#include "keyboard/code_convert.h"
#include "keyboard/code.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 内部用キーコードをVIALのキーコードに変換する。
 * @param keycode_internal 変換する内部用キーコード
 * @param keycode_vial 変換後のVIALのキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
bool code_convert_to_vial(icode_t keycode_internal, uint16_t *keycode_vial) {
  if (!(keycode_internal == ISC_BOOT || keycode_internal == IKC_NOOP ||
        (keycode_internal >= IKC_A && keycode_internal <= IMKC_RIGHT_GUI))) {
    return false;
  }

  if (keycode_internal == ISC_BOOT) {
    *keycode_vial = VIAL_KEYCODE_BOOTLOADER;
    return true;
  }

  *keycode_vial = (uint16_t)keycode_internal;
  return true;
}

/**
 * @brief VIALのキーコードを内部用キーコードに変換する。
 * @param keycode_vial 変換するVIALのキーコード
 * @param keycode_internal 変換後の内部用のキーコードを格納するポインタ
 * @return 変換に成功した場合はtrue、失敗した場合はfalse
 */
bool code_convert_to_internal(uint16_t keycode_vial,
                              icode_t *keycode_internal) {
  if (!(keycode_vial == VIAL_KEYCODE_BOOTLOADER || keycode_vial == IKC_NOOP ||
        (keycode_vial >= IKC_A && keycode_vial <= IMKC_RIGHT_GUI))) {
    return false;
  }

  if (keycode_vial == VIAL_KEYCODE_BOOTLOADER) {
    *keycode_internal = ISC_BOOT;
    return true;
  }
  *keycode_internal = (icode_t)keycode_vial;
  return true;
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
