#ifndef PWMK_CODE_H
#define PWMK_CODE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 内部キーコードの型。
 *
 * 上位8bitがopcode、次の8bitがmodeまたはflags、下位16bitがoperandを表す。
 * [oooooooo][mmmmmmmm][xxxxxxxxxxxxxxxx]
 *  ^ bit 31 (MSB)                     ^ bit 0 (LSB)
 */
typedef uint32_t icode_t;

/**
 * @brief 内部キーコードを構成する。
 * @param opcode アクションの種類
 * @param mode opcode固有のmodeまたはflags
 * @param operand opcode固有のoperand
 * @return パックされた内部キーコード
 */
#define ICODE_PACK(opcode, mode, operand)                                      \
  ((((uint32_t)(opcode) & UINT32_C(0xFF)) << 24) |                             \
   (((uint32_t)(mode) & UINT32_C(0xFF)) << 16) |                               \
   ((uint32_t)(operand) & UINT32_C(0xFFFF)))

#define ICODE_OPCODE(code) ((uint8_t)(((uint32_t)(code) >> 24) & 0xFFu))
#define ICODE_MODE(code) ((uint8_t)(((uint32_t)(code) >> 16) & 0xFFu))
#define ICODE_OPERAND(code) ((uint16_t)((uint32_t)(code) & 0xFFFFu))

// clang-format off

// 内部キーコードのopcode。
#define ICODE_OPCODE_NOOP               UINT8_C(0x00)
#define ICODE_OPCODE_TRANSPARENT        UINT8_C(0x01)
#define ICODE_OPCODE_KEYBOARD           UINT8_C(0x10)
#define ICODE_OPCODE_MODIFIER           UINT8_C(0x11)
#define ICODE_OPCODE_CONSUMER           UINT8_C(0x12)
#define ICODE_OPCODE_POINTING           UINT8_C(0x13)
#define ICODE_OPCODE_SYSTEM             UINT8_C(0x14)
#define ICODE_OPCODE_LAYER              UINT8_C(0x20)
#define ICODE_OPCODE_MACRO              UINT8_C(0x21)
#define ICODE_OPCODE_SEQUENCE           UINT8_C(0x22)
#define ICODE_OPCODE_USER_MIN           UINT8_C(0x80)
#define ICODE_OPCODE_USER_MAX           UINT8_C(0xBF)
#define ICODE_OPCODE_INVALID            UINT8_C(0xFF)

#define ICODE_NOOP                      ICODE_PACK(ICODE_OPCODE_NOOP, 0, 0)
#define ICODE_TRANSPARENT               ICODE_PACK(ICODE_OPCODE_TRANSPARENT, 0, 0)

// ポインティングデバイスopcodeのmodeとoperand。
#define ICODE_POINTING_BUTTON           UINT8_C(0x01)
#define ICODE_POINTING_MOVE_X           UINT8_C(0x02)
#define ICODE_POINTING_MOVE_Y           UINT8_C(0x03)
#define ICODE_POINTING_WHEEL            UINT8_C(0x04)
#define ICODE_POINTING_PRED             UINT16_C(0x0001)
#define ICODE_POINTING_SUCC             UINT16_C(0xFFFF)

// システムopcodeのmodeとoperand。
#define ICODE_SYSTEM_BOOT               UINT8_C(0x01)
#define ICODE_SYSTEM_CONNECTION         UINT8_C(0x02)
#define ICODE_SYSTEM_BLE                UINT8_C(0x03)
#define ICODE_CONNECTION_TOGGLE         UINT16_C(0x0001)
#define ICODE_CONNECTION_USB            UINT16_C(0x0002)
#define ICODE_CONNECTION_BLE            UINT16_C(0x0003)
#define ICODE_BLE_UNPAIR                UINT16_C(0x0000)
#define ICODE_BLE_SLOT_1                UINT16_C(0x0001)
#define ICODE_BLE_SLOT_2                UINT16_C(0x0002)
#define ICODE_BLE_SLOT_3                UINT16_C(0x0003)
#define ICODE_BLE_SLOT_4                UINT16_C(0x0004)

// 標準キーボードキー。operandはUSB HID Usage ID、modeは修飾子ビット。
#define IKC_NOOP                        ICODE_NOOP
#define IKC_A                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x04)
#define IKC_B                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x05)
#define IKC_C                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x06)
#define IKC_D                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x07)
#define IKC_E                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x08)
#define IKC_F                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x09)
#define IKC_G                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x0A)
#define IKC_H                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x0B)
#define IKC_I                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x0C)
#define IKC_J                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x0D)
#define IKC_K                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x0E)
#define IKC_L                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x0F)
#define IKC_M                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x10)
#define IKC_N                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x11)
#define IKC_O                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x12)
#define IKC_P                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x13)
#define IKC_Q                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x14)
#define IKC_R                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x15)
#define IKC_S                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x16)
#define IKC_T                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x17)
#define IKC_U                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x18)
#define IKC_V                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x19)
#define IKC_W                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x1A)
#define IKC_X                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x1B)
#define IKC_Y                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x1C)
#define IKC_Z                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x1D)
#define IKC_1                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x1E)
#define IKC_2                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x1F)
#define IKC_3                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x20)
#define IKC_4                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x21)
#define IKC_5                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x22)
#define IKC_6                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x23)
#define IKC_7                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x24)
#define IKC_8                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x25)
#define IKC_9                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x26)
#define IKC_0                           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x27)
#define IKC_ENTER                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x28)
#define IKC_ESCAPE                      ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x29)
#define IKC_BACKSPACE                   ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x2A)
#define IKC_TAB                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x2B)
#define IKC_SPACE                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x2C)
#define IKC_MINUS                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x2D)
#define IKC_EQUAL                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x2E)
#define IKC_LEFT_BRACKET                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x2F)
#define IKC_RIGHT_BRACKET               ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x30)
#define IKC_BACKSLASH                   ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x31)
#define IKC_NON_US_HASH                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x32)
#define IKC_SEMICOLON                   ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x33)
#define IKC_SINGLE_QUOTE                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x34)
#define IKC_GRAVE                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x35)
#define IKC_COMMA                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x36)
#define IKC_DOT                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x37)
#define IKC_SLASH                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x38)
#define IKC_CAPSLOCK                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x39)
#define IKC_F1                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x3A)
#define IKC_F2                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x3B)
#define IKC_F3                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x3C)
#define IKC_F4                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x3D)
#define IKC_F5                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x3E)
#define IKC_F6                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x3F)
#define IKC_F7                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x40)
#define IKC_F8                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x41)
#define IKC_F9                          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x42)
#define IKC_F10                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x43)
#define IKC_F11                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x44)
#define IKC_F12                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x45)
#define IKC_PRINT_SCREEN                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x46)
#define IKC_SCROLL_LOCK                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x47)
#define IKC_PAUSE                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x48)
#define IKC_INSERT                      ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x49)
#define IKC_HOME                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x4A)
#define IKC_PAGE_UP                     ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x4B)
#define IKC_DELETE                      ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x4C)
#define IKC_END                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x4D)
#define IKC_PAGE_DOWN                   ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x4E)
#define IKC_RIGHT_ARROW                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x4F)
#define IKC_LEFT_ARROW                  ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x50)
#define IKC_DOWN_ARROW                  ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x51)
#define IKC_UP_ARROW                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x52)
#define IKC_NUM_LOCK                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x53)
#define IKC_KEYPAD_SLASH                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x54)
#define IKC_KEYPAD_ASTERISK             ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x55)
#define IKC_KEYPAD_MINUS                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x56)
#define IKC_KEYPAD_PLUS                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x57)
#define IKC_KEYPAD_ENTER                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x58)
#define IKC_KEYPAD_1                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x59)
#define IKC_KEYPAD_2                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x5A)
#define IKC_KEYPAD_3                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x5B)
#define IKC_KEYPAD_4                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x5C)
#define IKC_KEYPAD_5                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x5D)
#define IKC_KEYPAD_6                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x5E)
#define IKC_KEYPAD_7                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x5F)
#define IKC_KEYPAD_8                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x60)
#define IKC_KEYPAD_9                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x61)
#define IKC_KEYPAD_0                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x62)
#define IKC_KEYPAD_DOT                  ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x63)
#define IKC_NON_US_BACKSLASH            ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x64)
#define IKC_APPLICATION                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x65)
#define IKC_POWER                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x66)
#define IKC_KEYPAD_EQUAL                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x67)
#define IKC_F13                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x68)
#define IKC_F14                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x69)
#define IKC_F15                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x6A)
#define IKC_F16                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x6B)
#define IKC_F17                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x6C)
#define IKC_F18                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x6D)
#define IKC_F19                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x6E)
#define IKC_F20                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x6F)
#define IKC_F21                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x70)
#define IKC_F22                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x71)
#define IKC_F23                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x72)
#define IKC_F24                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x73)
#define IKC_EXECUTE                     ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x74)
#define IKC_HELP                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x75)
#define IKC_MENU                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x76)
#define IKC_SELECT                      ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x77)
#define IKC_STOP                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x78)
#define IKC_AGAIN                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x79)
#define IKC_UNDO                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x7A)
#define IKC_CUT                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x7B)
#define IKC_COPY                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x7C)
#define IKC_PASTE                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x7D)
#define IKC_FIND                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x7E)
#define IKC_MUTE                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x7F)
#define IKC_VOLUME_UP                   ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x80)
#define IKC_VOLUME_DOWN                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x81)
#define IKC_LOCKING_CAPS                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x82)
#define IKC_LOCKING_NUM                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x83)
#define IKC_LOCKING_SCROLL              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x84)
#define IKC_KEYPAD_COMMA                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x85)
#define IKC_KEYPAD_EQUAL_UNIX           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x86)
#define IKC_INTERNATIONAL1              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x87)
#define IKC_INTERNATIONAL2              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x88)
#define IKC_INTERNATIONAL3              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x89)
#define IKC_INTERNATIONAL4              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x8A)
#define IKC_INTERNATIONAL5              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x8B)
#define IKC_INTERNATIONAL6              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x8C)
#define IKC_INTERNATIONAL7              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x8D)
#define IKC_INTERNATIONAL8              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x8E)
#define IKC_INTERNATIONAL9              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x8F)
#define IKC_LANG1                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x90)
#define IKC_LANG2                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x91)
#define IKC_LANG3                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x92)
#define IKC_LANG4                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x93)
#define IKC_LANG5                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x94)
#define IKC_LANG6                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x95)
#define IKC_LANG7                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x96)
#define IKC_LANG8                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x97)
#define IKC_LANG9                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x98)
#define IKC_ALTERNATE_ERASE             ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x99)
#define IKC_SYSREQ                      ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x9A)
#define IKC_CANCEL                      ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x9B)
#define IKC_CLEAR                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x9C)
#define IKC_PRIOR                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x9D)
#define IKC_RETURN2                     ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x9E)
#define IKC_SEPARATOR                   ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0x9F)
#define IKC_OUT                         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xA0)
#define IKC_OPER                        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xA1)
#define IKC_CLEAR_AGAIN                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xA2)
#define IKC_CRSEL                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xA3)
#define IKC_EXSEL                       ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xA4)
#define IKC_KEYPAD_00                   ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB0)
#define IKC_KEYPAD_000                  ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB1)
#define IKC_THOUSANDS_SEPARATOR         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB2)
#define IKC_DECIMAL_SEPARATOR           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB3)
#define IKC_CURRENCY_UNIT               ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB4)
#define IKC_CURRENCY_SUB_UNIT           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB5)
#define IKC_KEYPAD_LEFT_PAREN           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB6)
#define IKC_KEYPAD_RIGHT_PAREN          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB7)
#define IKC_KEYPAD_LEFT_BRACE           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB8)
#define IKC_KEYPAD_RIGHT_BRACE          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xB9)
#define IKC_KEYPAD_TAB                  ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xBA)
#define IKC_KEYPAD_BACKSPACE            ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xBB)
#define IKC_KEYPAD_A                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xBC)
#define IKC_KEYPAD_B                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xBD)
#define IKC_KEYPAD_C                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xBE)
#define IKC_KEYPAD_D                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xBF)
#define IKC_KEYPAD_E                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC0)
#define IKC_KEYPAD_F                    ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC1)
#define IKC_KEYPAD_XOR                  ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC2)
#define IKC_KEYPAD_CARET                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC3)
#define IKC_KEYPAD_PERCENT              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC4)
#define IKC_KEYPAD_LESS                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC5)
#define IKC_KEYPAD_GREATER              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC6)
#define IKC_KEYPAD_AMPERSAND            ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC7)
#define IKC_KEYPAD_DOUBLE_AMPERSAND     ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC8)
#define IKC_KEYPAD_VERTICAL_BAR         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xC9)
#define IKC_KEYPAD_DOUBLE_VERTICAL_BAR  ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xCA)
#define IKC_KEYPAD_COLON                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xCB)
#define IKC_KEYPAD_HASH                 ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xCC)
#define IKC_KEYPAD_SPACE                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xCD)
#define IKC_KEYPAD_AT                   ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xCE)
#define IKC_KEYPAD_EXCLAMATION          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xCF)
#define IKC_KEYPAD_MEMORY_STORE         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD0)
#define IKC_KEYPAD_MEMORY_RECALL        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD1)
#define IKC_KEYPAD_MEMORY_CLEAR         ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD2)
#define IKC_KEYPAD_MEMORY_ADD           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD3)
#define IKC_KEYPAD_MEMORY_SUBTRACT      ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD4)
#define IKC_KEYPAD_MEMORY_MULTIPLY      ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD5)
#define IKC_KEYPAD_MEMORY_DIVIDE        ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD6)
#define IKC_KEYPAD_PLUS_MINUS           ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD7)
#define IKC_KEYPAD_CLEAR                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD8)
#define IKC_KEYPAD_CLEAR_ENTRY          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xD9)
#define IKC_KEYPAD_BINARY               ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xDA)
#define IKC_KEYPAD_OCTAL                ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xDB)
#define IKC_KEYPAD_DECIMAL              ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xDC)
#define IKC_KEYPAD_HEXADECIMAL          ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xDD)

// 単独の修飾キー。modeが修飾子ビット、operandは0。
#define IMKC_LEFT_CONTROL               ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x01, 0)
#define IMKC_LEFT_SHIFT                 ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x02, 0)
#define IMKC_LEFT_ALT                   ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x04, 0)
#define IMKC_LEFT_GUI                   ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x08, 0)
#define IMKC_RIGHT_CONTROL              ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x10, 0)
#define IMKC_RIGHT_SHIFT                ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x20, 0)
#define IMKC_RIGHT_ALT                  ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x40, 0)
#define IMKC_RIGHT_GUI                  ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x80, 0)

// コンシューマーキー。operandはコンシューマーHID Usage。
#define ICC_RECORD                      ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00B2)
#define ICC_FAST_FORWARD                ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00B3)
#define ICC_REWIND                      ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00B4)
#define ICC_NEXT_TRACK                  ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00B5)
#define ICC_PREV_TRACK                  ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00B6)
#define ICC_STOP_TRACK                  ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00B7)
#define ICC_EJECT                       ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00B8)
#define ICC_RANDOM_PLAY                 ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00B9)
#define ICC_STOP_EJECT                  ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00CC)
#define ICC_PLAY_PAUSE                  ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00CD)
#define ICC_VOL_MUTE                    ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00E2)
#define ICC_VOL_UP                      ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00E9)
#define ICC_VOL_DOWN                    ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x00EA)

// ポインティングデバイスキー。
#define IMC_MOUSE_LEFT                  ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_BUTTON, 1)
#define IMC_MOUSE_RIGHT                 ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_BUTTON, 2)
#define IMC_MOUSE_MIDDLE                ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_BUTTON, 4)
#define IMC_MOUSE_MOVE_UP               ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_MOVE_Y, ICODE_POINTING_SUCC)
#define IMC_MOUSE_MOVE_DOWN             ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_MOVE_Y, ICODE_POINTING_PRED)
#define IMC_MOUSE_MOVE_LEFT             ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_MOVE_X, ICODE_POINTING_SUCC)
#define IMC_MOUSE_MOVE_RIGHT            ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_MOVE_X, ICODE_POINTING_PRED)
#define IMC_MOUSE_WHEEL_UP              ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_WHEEL, ICODE_POINTING_PRED)
#define IMC_MOUSE_WHEEL_DOWN            ICODE_PACK(ICODE_OPCODE_POINTING, ICODE_POINTING_WHEEL, ICODE_POINTING_SUCC)

// システム操作。
#define ISC_BOOT                        ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_BOOT, 0)
#define ISC_CONN_TOGGLE                 ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_CONNECTION, 1)
#define ISC_CONN_USB                    ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_CONNECTION, 2)
#define ISC_CONN_BLE                    ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_CONNECTION, 3)
#define ISC_BLE_UNPAIR                  ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_BLE, 0)
#define ISC_BLE_SLOT_1                  ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_BLE, 1)
#define ISC_BLE_SLOT_2                  ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_BLE, 2)
#define ISC_BLE_SLOT_3                  ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_BLE, 3)
#define ISC_BLE_SLOT_4                  ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_BLE, 4)

#define IUC_RANGE_MIN                   ICODE_PACK(ICODE_OPCODE_USER_MIN, 0, 0)
#define IUC_RANGE_MAX                   ICODE_PACK(ICODE_OPCODE_USER_MAX, UINT8_MAX, UINT16_MAX)

// clang-format on

typedef uint8_t code_t;

typedef enum {
  KMC_UNDEFINED = 0x00,
  KMC_LEFT_CONTROL = 0x01,
  KMC_LEFT_SHIFT = 0x02,
  KMC_LEFT_ALT = 0x04,
  KMC_LEFT_GUI = 0x08,
  KMC_RIGHT_CONTROL = 0x10,
  KMC_RIGHT_SHIFT = 0x20,
  KMC_RIGHT_ALT = 0x40,
  KMC_RIGHT_GUI = 0x80,
} code_mod_t;

typedef uint8_t code_mod_bits_t;

typedef enum {
  CC_UNDEFINED = 0x0000,
  CC_RECORD = 0x00B2,
  CC_FAST_FORWARD = 0x00B3,
  CC_REWIND = 0x00B4,
  CC_NEXT_TRACK = 0x00B5,
  CC_PREV_TRACK = 0x00B6,
  CC_STOP_TRACK = 0x00B7,
  CC_EJECT = 0x00B8,
  CC_RANDOM_PLAY = 0x00B9,
  CC_STOP_EJECT = 0x00CC,
  CC_PLAY_PAUSE = 0x00CD,
  CC_VOL_MUTE = 0x00E2,
  CC_VOL_UP = 0x00E9,
  CC_VOL_DOWN = 0x00EA,
  CC_RANGE_MAX = 0xFFFF,
} code_consumer_t;

typedef enum {
  MBC_UNDEFINED = 0x00,
  MBC_LEFT = 0x01,
  MBC_RIGHT = 0x02,
  MBC_MIDDLE = 0x04,
} code_mouse_button_t;

bool code_icode_is_valid(icode_t icode);
code_mod_t code_icode_to_modifier(icode_t icode);
code_mod_bits_t code_icode_extract_modifier_bits(icode_t icode);
code_consumer_t code_icodes_to_consumer(icode_t icode);
code_mouse_button_t code_icodes_to_mouse_button(icode_t icode);

#define APPLY_MOD(icode, modifier)                                             \
  ICODE_PACK(ICODE_OPCODE_KEYBOARD,                                            \
             ((ICODE_MODE(icode) | (modifier)) & UINT8_C(0xFF)),               \
             ICODE_OPERAND(icode))

#define LEFT_CTRL(icode) APPLY_MOD(icode, KMC_LEFT_CONTROL)
#define LEFT_SHIFT(icode) APPLY_MOD(icode, KMC_LEFT_SHIFT)
#define LEFT_ALT(icode) APPLY_MOD(icode, KMC_LEFT_ALT)
#define LEFT_GUI(icode) APPLY_MOD(icode, KMC_LEFT_GUI)
#define RIGHT_CTRL(icode) APPLY_MOD(icode, KMC_RIGHT_CONTROL)
#define RIGHT_SHIFT(icode) APPLY_MOD(icode, KMC_RIGHT_SHIFT)
#define RIGHT_ALT(icode) APPLY_MOD(icode, KMC_RIGHT_ALT)
#define RIGHT_GUI(icode) APPLY_MOD(icode, KMC_RIGHT_GUI)

#endif // PWMK_CODE_H
