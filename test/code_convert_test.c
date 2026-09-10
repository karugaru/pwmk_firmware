#include "keyboard/code_convert.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

// テスト補助関数
static void code_convert_test_assert_round_trip(icode_t internal,
                                                uint16_t expected_vial) {
  uint16_t vial;
  icode_t converted_internal;

  TEST_ASSERT_TRUE(code_convert_to_vial(internal, &vial));
  TEST_ASSERT_EQUAL_HEX16(expected_vial, vial);
  TEST_ASSERT_TRUE(code_convert_to_internal(vial, &converted_internal));
  TEST_ASSERT_EQUAL_UINT32(internal, converted_internal);
}

static void code_convert_test_assert_vial_noop(icode_t internal) {
  uint16_t vial;

  TEST_ASSERT_TRUE(code_convert_to_vial(internal, &vial));
  TEST_ASSERT_EQUAL_HEX16(IKC_NOOP, vial);
}

static void test_code_internal_format(void) {
  const icode_t modified = LEFT_ALT(LEFT_CTRL(IKC_F10));

  TEST_ASSERT_EQUAL_UINT32(0x10050043, modified);
  TEST_ASSERT_EQUAL_UINT8(ICODE_OPCODE_KEYBOARD, ICODE_OPCODE(modified));
  TEST_ASSERT_EQUAL_UINT8(KMC_LEFT_CONTROL | KMC_LEFT_ALT,
                          ICODE_MODE(modified));
  TEST_ASSERT_EQUAL_UINT16(0x0043, ICODE_OPERAND(modified));
  TEST_ASSERT_EQUAL_UINT32(0x11020000, IMKC_LEFT_SHIFT);
  TEST_ASSERT_EQUAL_UINT32(0x120000E9, ICC_VOL_UP);
  TEST_ASSERT_EQUAL_UINT32(0x13030001, IMC_MOUSE_MOVE_DOWN);
  TEST_ASSERT_EQUAL_UINT32(0x14020003, ISC_CONN_BLE);
}

static void test_code_validation(void) {
  TEST_ASSERT_TRUE(code_icode_is_valid(IKC_A));
  TEST_ASSERT_TRUE(code_icode_is_valid(IMKC_LEFT_SHIFT));
  TEST_ASSERT_TRUE(code_icode_is_valid(ICC_VOL_UP));
  TEST_ASSERT_TRUE(code_icode_is_valid(IMC_MOUSE_MOVE_UP));
  TEST_ASSERT_TRUE(code_icode_is_valid(ISC_BLE_SLOT_4));
  TEST_ASSERT_TRUE(code_icode_is_valid(ICODE_TRANSPARENT));
  TEST_ASSERT_TRUE(code_icode_is_valid(IUC_RANGE_MIN));

  TEST_ASSERT_FALSE(code_icode_is_valid(ICODE_PACK(ICODE_OPCODE_NOOP, 0, 1)));
  TEST_ASSERT_FALSE(
      code_icode_is_valid(ICODE_PACK(ICODE_OPCODE_KEYBOARD, 0, 0xA5)));
  TEST_ASSERT_FALSE(
      code_icode_is_valid(ICODE_PACK(ICODE_OPCODE_MODIFIER, 0x03, 0)));
  TEST_ASSERT_FALSE(
      code_icode_is_valid(ICODE_PACK(ICODE_OPCODE_CONSUMER, 0, 0x0001)));
  TEST_ASSERT_FALSE(
      code_icode_is_valid(ICODE_PACK(ICODE_OPCODE_POINTING, 0x05, 1)));
  TEST_ASSERT_FALSE(code_icode_is_valid(
      ICODE_PACK(ICODE_OPCODE_SYSTEM, ICODE_SYSTEM_CONNECTION, 4)));
  TEST_ASSERT_FALSE(code_icode_is_valid(ICODE_PACK(0x23, 0, 0)));
  TEST_ASSERT_FALSE(
      code_icode_is_valid(ICODE_PACK(ICODE_OPCODE_INVALID, 0, 0)));
}

static void test_code_convert_standard_keycodes(void) {
  const struct {
    icode_t internal;
    uint16_t vial;
  } test_cases[] = {
      {IKC_A, 0x0004},    {IKC_F24, 0x0073},       {IKC_POWER, 0x0066},
      {IKC_MUTE, 0x007F}, {IKC_VOLUME_UP, 0x0080}, {IKC_VOLUME_DOWN, 0x0081},
  };

  for (size_t index = 0; index < sizeof(test_cases) / sizeof(test_cases[0]);
       index++) {
    code_convert_test_assert_round_trip(test_cases[index].internal,
                                        test_cases[index].vial);
  }
}

static void test_code_convert_noop(void) {
  code_convert_test_assert_round_trip(IKC_NOOP, IKC_NOOP);
}

static void test_code_convert_modified_keycodes(void) {
  code_convert_test_assert_round_trip(LEFT_ALT(LEFT_CTRL(IKC_F10)), 0x0543);
  code_convert_test_assert_round_trip(RIGHT_SHIFT(RIGHT_CTRL(IKC_A)), 0x1304);
  code_convert_test_assert_round_trip(RIGHT_ALT(RIGHT_CTRL(IKC_A)), 0x1504);
  code_convert_test_assert_round_trip(IMKC_LEFT_CONTROL, 0x00E0);
  code_convert_test_assert_round_trip(IMKC_RIGHT_GUI, 0x00E7);
}

static void test_code_convert_consumer_keycodes(void) {
  const struct {
    icode_t internal;
    uint16_t vial;
  } test_cases[] = {
      {ICC_FAST_FORWARD, 0x00BB}, {ICC_REWIND, 0x00BC},
      {ICC_NEXT_TRACK, 0x00AB},   {ICC_PREV_TRACK, 0x00AC},
      {ICC_STOP_TRACK, 0x00AD},   {ICC_EJECT, 0x00B0},
      {ICC_PLAY_PAUSE, 0x00AE},   {ICC_VOL_MUTE, 0x00A8},
      {ICC_VOL_UP, 0x00A9},       {ICC_VOL_DOWN, 0x00AA},
  };

  for (size_t index = 0; index < sizeof(test_cases) / sizeof(test_cases[0]);
       index++) {
    code_convert_test_assert_round_trip(test_cases[index].internal,
                                        test_cases[index].vial);
  }

  code_convert_test_assert_vial_noop(ICC_RECORD);
  code_convert_test_assert_vial_noop(ICC_RANDOM_PLAY);
  code_convert_test_assert_vial_noop(ICC_STOP_EJECT);
}

static void test_code_convert_mouse_keycodes(void) {
  const struct {
    icode_t internal;
    uint16_t vial;
  } test_cases[] = {
      {IMC_MOUSE_LEFT, 0x00D1},       {IMC_MOUSE_RIGHT, 0x00D2},
      {IMC_MOUSE_MIDDLE, 0x00D3},     {IMC_MOUSE_MOVE_UP, 0x00CD},
      {IMC_MOUSE_MOVE_DOWN, 0x00CE},  {IMC_MOUSE_MOVE_LEFT, 0x00CF},
      {IMC_MOUSE_MOVE_RIGHT, 0x00D0}, {IMC_MOUSE_WHEEL_UP, 0x00D9},
      {IMC_MOUSE_WHEEL_DOWN, 0x00DA},
  };

  for (size_t index = 0; index < sizeof(test_cases) / sizeof(test_cases[0]);
       index++) {
    code_convert_test_assert_round_trip(test_cases[index].internal,
                                        test_cases[index].vial);
  }
}

static void test_code_convert_special_and_user_keycodes(void) {
  const struct {
    icode_t internal;
    uint16_t vial;
  } test_cases[] = {
      {ISC_BOOT, VIAL_KEYCODE_BOOTLOADER},
      {ISC_CONN_TOGGLE, 0x7781},
      {ISC_CONN_USB, 0x7780},
      {ISC_CONN_BLE, 0x7786},
      {ISC_BLE_UNPAIR, 0x7792},
      {ISC_BLE_SLOT_1, 0x7793},
      {ISC_BLE_SLOT_2, 0x7794},
      {ISC_BLE_SLOT_3, 0x7795},
      {ISC_BLE_SLOT_4, 0x7796},
      {IUC_RANGE_MIN, VIAL_KEYCODE_USER_START},
      {IUC_RANGE_MIN + 31, VIAL_KEYCODE_USER_END},
  };

  for (size_t index = 0; index < sizeof(test_cases) / sizeof(test_cases[0]);
       index++) {
    code_convert_test_assert_round_trip(test_cases[index].internal,
                                        test_cases[index].vial);
  }
}

static void test_code_convert_rejects_unrepresentable_keycodes(void) {
  uint16_t vial;
  icode_t internal;

  TEST_ASSERT_FALSE(code_convert_to_vial(LEFT_CTRL(RIGHT_ALT(IKC_A)), &vial));
  code_convert_test_assert_vial_noop(IKC_KEYPAD_00);
  code_convert_test_assert_vial_noop(IKC_KEYPAD_HEXADECIMAL);
  code_convert_test_assert_vial_noop(IUC_RANGE_MIN + 32);
  code_convert_test_assert_vial_noop(ICODE_PACK(ICODE_OPCODE_USER_MIN, 1, 0));
  code_convert_test_assert_vial_noop(
      ICODE_PACK(ICODE_OPCODE_USER_MIN + 1, 0, 0));
  TEST_ASSERT_FALSE(code_convert_to_internal(0x1004, &internal));
  TEST_ASSERT_FALSE(code_convert_to_internal(0x00B1, &internal));
  TEST_ASSERT_FALSE(code_convert_to_internal(0x00DE, &internal));
  TEST_ASSERT_FALSE(code_convert_to_internal(0x00D4, &internal));
  TEST_ASSERT_FALSE(code_convert_to_internal(0x7797, &internal));
  TEST_ASSERT_FALSE(code_convert_to_internal(0x2000, &internal));
  TEST_ASSERT_FALSE(
      code_convert_to_internal(VIAL_KEYCODE_USER_END + 1, &internal));
}

static void test_code_convert_identifies_bootloader_as_dangerous(void) {
  TEST_ASSERT_TRUE(
      code_convert_is_dangerous_vial_code(VIAL_KEYCODE_BOOTLOADER));
  TEST_ASSERT_FALSE(
      code_convert_is_dangerous_vial_code(VIAL_KEYCODE_USER_START));
}

int main(void) {
  UNITY_BEGIN();

  // テストケース: 内部32bit形式のパックと展開
  RUN_TEST(test_code_internal_format);
  // テストケース: opcodeごとの妥当性判定
  RUN_TEST(test_code_validation);
  // テストケース: 標準キーコードの変換
  RUN_TEST(test_code_convert_standard_keycodes);
  // テストケース: NOOPの変換
  RUN_TEST(test_code_convert_noop);
  // テストケース: 修飾キー付きの変換
  RUN_TEST(test_code_convert_modified_keycodes);
  // テストケース: コンシューマーキーコードの変換
  RUN_TEST(test_code_convert_consumer_keycodes);
  // テストケース: マウスキーコードの変換
  RUN_TEST(test_code_convert_mouse_keycodes);
  // テストケース: 特殊キーコードとユーザー拡張コードの変換
  RUN_TEST(test_code_convert_special_and_user_keycodes);
  // テストケース: 変換不可能なキーコードの拒否
  RUN_TEST(test_code_convert_rejects_unrepresentable_keycodes);
  // テストケース: ブートローダーキーコードの危険性の判定
  RUN_TEST(test_code_convert_identifies_bootloader_as_dangerous);

  return UNITY_END();
}
