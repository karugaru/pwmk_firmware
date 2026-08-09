#ifndef PWMK_CODE_CONVERT_H
#define PWMK_CODE_CONVERT_H

#include "keyboard/code.h"
#include <stdbool.h>
#include <stdint.h>

#define VIAL_KEYCODE_BOOTLOADER 0x7C00// ブートローダーキーのVIALキーコード

bool code_convert_to_vial(icode_t keycode_internal, uint16_t *keycode_vial);
bool code_convert_to_internal(uint16_t keycode_vial, icode_t *keycode_internal);
bool code_convert_is_dangerous_vial_code(uint16_t keycode_vial);

#endif // PWMK_CODE_CONVERT_H
