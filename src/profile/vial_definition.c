#include "profile/vial_definition.h"

// VIALのキーボード定義データのバイト列
const uint8_t vial_keyboard_definition[VIAL_KEYBOARD_DEFINITION_SIZE] =
    VIAL_KEYBOARD_DEFINITION_BYTES;

// VIALのアンロックコンボの定義。各要素は
// {行番号, 列番号}のペアで、アンロックコンボを構成するキーの位置を表す。
const uint8_t vial_unlock_combo[VIAL_UNLOCK_COMBO_LENGTH][2] =
    VIAL_UNLOCK_COMBO;
