#include "keyboard/code.h"
#include <stdbool.h>

#define CODE_MODIFIER_MASK                                                     \
  (KMC_LEFT_CONTROL | KMC_LEFT_SHIFT | KMC_LEFT_ALT | KMC_LEFT_GUI |           \
   KMC_RIGHT_CONTROL | KMC_RIGHT_SHIFT | KMC_RIGHT_ALT | KMC_RIGHT_GUI)

#define IS_SINGLE_FLAG(x) (((x) != 0) && (((x) & (uint8_t)((x) - 1)) == 0))
#define IS_ONLY_MODIFIER(x) (((x) & (uint8_t)~CODE_MODIFIER_MASK) == 0)
/*
 * 内部関数宣言
 */

static bool _code_is_valid_keyboard_usage(uint16_t usage);
static bool _code_is_valid_consumer_usage(uint16_t usage);
static bool _code_is_valid_pointing_action(uint8_t mode, uint16_t operand);
static bool _code_is_valid_system_action(uint8_t mode, uint16_t operand);

/*
 * 公開関数
 */

/**
 * @brief 内部キーコードが妥当なアクション記述子か判定する。
 * @param icode 判定する内部キーコード
 * @return 変換後の修飾子コード、該当しない場合はKMC_UNDEFINED
 */
bool code_icode_is_valid(icode_t icode) {
  const uint8_t opcode = ICODE_OPCODE(icode);
  const uint8_t mode = ICODE_MODE(icode);
  const uint16_t operand = ICODE_OPERAND(icode);

  switch (opcode) {
  case ICODE_OPCODE_NOOP:
    return icode == ICODE_NOOP;
  case ICODE_OPCODE_TRANSPARENT:
    return icode == ICODE_TRANSPARENT;
  case ICODE_OPCODE_KEYBOARD:
    // 修飾ビットが壊れていなくて、Usageが有効であることを確認する
    return IS_ONLY_MODIFIER(mode) && _code_is_valid_keyboard_usage(operand);
  case ICODE_OPCODE_MODIFIER:
    // 修飾ビットが壊れていなくて、単一の修飾子であることを確認する
    return operand == 0 && mode != KMC_UNDEFINED && IS_SINGLE_FLAG(mode) &&
           IS_ONLY_MODIFIER(mode);
  case ICODE_OPCODE_CONSUMER:
    return mode == 0 && _code_is_valid_consumer_usage(operand);
  case ICODE_OPCODE_POINTING:
    return _code_is_valid_pointing_action(mode, operand);
  case ICODE_OPCODE_SYSTEM:
    return _code_is_valid_system_action(mode, operand);
  default:
    return opcode >= ICODE_OPCODE_USER_MIN && opcode <= ICODE_OPCODE_USER_MAX;
  }
}

/**
 * @brief 内部キーコードからキーボード修飾子コードに変換する。
 * @param icode 判定する内部キーコード
 * @return 変換後の修飾子コード、該当しない場合はKMC_UNDEFINED
 */
code_mod_t code_icode_to_modifier(icode_t icode) {
  if (ICODE_OPCODE(icode) != ICODE_OPCODE_MODIFIER ||
      ICODE_OPERAND(icode) != 0) {
    return KMC_UNDEFINED;
  }

  switch (ICODE_MODE(icode)) {
  case KMC_LEFT_CONTROL:
    return KMC_LEFT_CONTROL;
  case KMC_LEFT_SHIFT:
    return KMC_LEFT_SHIFT;
  case KMC_LEFT_ALT:
    return KMC_LEFT_ALT;
  case KMC_LEFT_GUI:
    return KMC_LEFT_GUI;
  case KMC_RIGHT_CONTROL:
    return KMC_RIGHT_CONTROL;
  case KMC_RIGHT_SHIFT:
    return KMC_RIGHT_SHIFT;
  case KMC_RIGHT_ALT:
    return KMC_RIGHT_ALT;
  case KMC_RIGHT_GUI:
    return KMC_RIGHT_GUI;
  default:
    return KMC_UNDEFINED;
  }
}

/**
 * @brief 内部キーコードからキーボード修飾子ビットを抽出する。
 * @param icode 判定する内部キーコード
 * @return 抽出した修飾子ビット、該当しない場合はKMC_UNDEFINED
 */
code_mod_bits_t code_icode_extract_modifier_bits(icode_t icode) {
  if (ICODE_OPCODE(icode) != ICODE_OPCODE_KEYBOARD ||
      !code_icode_is_valid(icode)) {
    return KMC_UNDEFINED;
  }

  return ICODE_MODE(icode);
}

/**
 * @brief 内部キーコードからコンシューマーコードに変換する。
 * @param icode 判定する内部キーコード
 * @return 変換後のコンシューマーコード、該当しない場合はCC_UNDEFINED
 */
code_consumer_t code_icodes_to_consumer(icode_t icode) {
  if (ICODE_OPCODE(icode) != ICODE_OPCODE_CONSUMER ||
      !code_icode_is_valid(icode)) {
    return CC_UNDEFINED;
  }

  return (code_consumer_t)ICODE_OPERAND(icode);
}

/**
 * @brief 内部キーコードからマウスボタンコードに変換する。
 * @param icode 判定する内部キーコード
 * @return 変換後のマウスボタンコード、該当しない場合はMBC_UNDEFINED
 */
code_mouse_button_t code_icodes_to_mouse_button(icode_t icode) {
  if (ICODE_OPCODE(icode) != ICODE_OPCODE_POINTING ||
      ICODE_MODE(icode) != ICODE_POINTING_BUTTON ||
      !code_icode_is_valid(icode)) {
    return MBC_UNDEFINED;
  }

  return (code_mouse_button_t)ICODE_OPERAND(icode);
}

/*
 * 内部関数
 */

/**
 * @brief キーボードUsageが内部形式で扱える値か判定する。
 * @param usage 判定するHID Usage ID
 * @return 扱えるUsageならtrue
 */
static bool _code_is_valid_keyboard_usage(uint16_t usage) {
  return (usage >= 0x04 && usage <= 0xA4) || (usage >= 0xB0 && usage <= 0xDD);
}

/**
 * @brief コンシューマーUsageが定義済みか判定する。
 * @param usage 判定するコンシューマーHID Usage
 * @return 定義済みならtrue
 */
static bool _code_is_valid_consumer_usage(uint16_t usage) {
  switch (usage) {
  case CC_RECORD:
  case CC_FAST_FORWARD:
  case CC_REWIND:
  case CC_NEXT_TRACK:
  case CC_PREV_TRACK:
  case CC_STOP_TRACK:
  case CC_EJECT:
  case CC_RANDOM_PLAY:
  case CC_STOP_EJECT:
  case CC_PLAY_PAUSE:
  case CC_VOL_MUTE:
  case CC_VOL_UP:
  case CC_VOL_DOWN:
    return true;
  default:
    return false;
  }
}

/**
 * @brief ポインティングアクションのmodeとoperandが妥当か判定する。
 * @param mode ポインティングアクションのmode
 * @param operand ポインティングアクションのoperand
 * @return 妥当ならtrue
 */
static bool _code_is_valid_pointing_action(uint8_t mode, uint16_t operand) {
  switch (mode) {
  case ICODE_POINTING_BUTTON:
    return operand == MBC_LEFT || operand == MBC_RIGHT || operand == MBC_MIDDLE;
  case ICODE_POINTING_MOVE_X:
  case ICODE_POINTING_MOVE_Y:
  case ICODE_POINTING_WHEEL:
    return operand == ICODE_POINTING_PRED || operand == ICODE_POINTING_SUCC;
  default:
    return false;
  }
}

/**
 * @brief システムアクションのmodeとoperandが妥当か判定する。
 * @param mode システムアクションのmode
 * @param operand システムアクションのoperand
 * @return 妥当ならtrue
 */
static bool _code_is_valid_system_action(uint8_t mode, uint16_t operand) {
  switch (mode) {
  case ICODE_SYSTEM_BOOT:
    return operand == 0;
  case ICODE_SYSTEM_CONNECTION:
    return operand >= ICODE_CONNECTION_TOGGLE &&
           operand <= ICODE_CONNECTION_BLE;
  case ICODE_SYSTEM_BLE:
    return operand <= ICODE_BLE_SLOT_4;
  default:
    return false;
  }
}
