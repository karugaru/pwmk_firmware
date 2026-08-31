#include "event_keyboard_test_setup.h"
#include "keyboard/event.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

// テスト補助関数
static void event_keyboard_test_process_key(icode_t keycode, bool pressed) {
  event_keyboard_test_set_keycode(keycode);
  event_process(0, 0, pressed, 0);
}

// テスト補助関数
static void event_keyboard_test_press_key(icode_t keycode) {
  event_keyboard_test_process_key(keycode, true);
}

// テスト補助関数
static void event_keyboard_test_release_key(icode_t keycode) {
  event_keyboard_test_process_key(keycode, false);
}

// テスト補助関数
static void event_keyboard_test_assert_report(
    const uint8_t expected[HID_KEYBOARD_REPORT_SIZE]) {
  event_hid_report_t report;

  TEST_ASSERT_TRUE(event_has_event());
  TEST_ASSERT_TRUE(event_pop_hid_report(&report));
  TEST_ASSERT_EQUAL_UINT8(HID_KEYBOARD_REPORT_ID, report.report_id);
  TEST_ASSERT_EQUAL_UINT16(HID_KEYBOARD_REPORT_SIZE, report.size);
  TEST_ASSERT_EQUAL_MEMORY(expected, report.data, HID_KEYBOARD_REPORT_SIZE);
}

static void event_keyboard_test_assert_consumer_report(
    const uint8_t expected[HID_CONSUMER_REPORT_SIZE]) {
  event_hid_report_t report;

  TEST_ASSERT_TRUE(event_has_event());
  TEST_ASSERT_TRUE(event_pop_hid_report(&report));
  TEST_ASSERT_EQUAL_UINT8(HID_CONSUMER_REPORT_ID, report.report_id);
  TEST_ASSERT_EQUAL_UINT16(HID_CONSUMER_REPORT_SIZE, report.size);
  TEST_ASSERT_EQUAL_MEMORY(expected, report.data, HID_CONSUMER_REPORT_SIZE);
}

// テスト補助関数
static void event_keyboard_test_assert_mouse_report(
    const uint8_t expected[HID_MOUSE_REPORT_SIZE]) {
  event_hid_report_t report;

  TEST_ASSERT_TRUE(event_has_event());
  TEST_ASSERT_TRUE(event_pop_hid_report(&report));
  TEST_ASSERT_EQUAL_UINT8(HID_MOUSE_REPORT_ID, report.report_id);
  TEST_ASSERT_EQUAL_UINT16(HID_MOUSE_REPORT_SIZE, report.size);
  TEST_ASSERT_EQUAL_MEMORY(expected, report.data, HID_MOUSE_REPORT_SIZE);
}

// テスト補助関数
static void event_keyboard_test_assert_no_event(void) {
  event_hid_report_t report;

  TEST_ASSERT_FALSE(event_has_event());
  TEST_ASSERT_FALSE(event_pop_hid_report(&report));
}

// テスト補助関数
static void event_keyboard_test_assert_platform_event(icode_t expected_keycode,
                                                      bool expected_pressed) {
  icode_t keycode;
  bool pressed;

  TEST_ASSERT_TRUE(event_keyboard_test_pop_platform_event(&keycode, &pressed));
  TEST_ASSERT_EQUAL_UINT32(expected_keycode, keycode);
  TEST_ASSERT_EQUAL_UINT8(expected_pressed, pressed);
  event_keyboard_test_assert_no_event();
}

static void test_event_keyboard_key_press_and_release(void) {
  const uint8_t pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      0, 0, (uint8_t)ICODE_OPERAND(IKC_A), 0, 0, 0, 0, 0,
  };
  const uint8_t released_report[HID_KEYBOARD_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(IKC_A);

  event_keyboard_test_press_key(IKC_A);
  event_keyboard_test_assert_report(pressed_report);
  event_keyboard_test_assert_no_event();

  event_keyboard_test_release_key(IKC_A);
  event_keyboard_test_assert_report(released_report);
  event_keyboard_test_assert_no_event();
}

static void test_event_keyboard_key_with_modifier(void) {
  const uint8_t pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      KMC_LEFT_SHIFT, 0, (uint8_t)ICODE_OPERAND(IKC_A), 0, 0, 0, 0, 0,
  };
  const uint8_t released_report[HID_KEYBOARD_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(LEFT_SHIFT(IKC_A));

  event_keyboard_test_press_key(LEFT_SHIFT(IKC_A));
  event_keyboard_test_assert_report(pressed_report);

  event_keyboard_test_release_key(LEFT_SHIFT(IKC_A));
  event_keyboard_test_assert_report(released_report);
}

static void test_event_keyboard_multiple_keys_and_release_order(void) {
  const uint8_t first_pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      0, 0, (uint8_t)ICODE_OPERAND(IKC_A), 0, 0, 0, 0, 0,
  };
  const uint8_t both_pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      0, 0, (uint8_t)ICODE_OPERAND(IKC_A), (uint8_t)ICODE_OPERAND(IKC_B), 0, 0,
      0, 0,
  };
  const uint8_t second_pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      0, 0, (uint8_t)ICODE_OPERAND(IKC_B), 0, 0, 0, 0, 0,
  };
  const uint8_t released_report[HID_KEYBOARD_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(IKC_A);

  event_keyboard_test_press_key(IKC_A);
  event_keyboard_test_assert_report(first_pressed_report);

  event_keyboard_test_press_key(IKC_B);
  event_keyboard_test_assert_report(both_pressed_report);

  event_keyboard_test_release_key(IKC_A);
  event_keyboard_test_assert_report(second_pressed_report);

  event_keyboard_test_release_key(IKC_B);
  event_keyboard_test_assert_report(released_report);
}

static void test_event_keyboard_duplicate_press_and_release(void) {
  const uint8_t pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      0, 0, (uint8_t)ICODE_OPERAND(IKC_A), 0, 0, 0, 0, 0,
  };
  const uint8_t released_report[HID_KEYBOARD_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(IKC_A);

  event_keyboard_test_press_key(IKC_A);
  event_keyboard_test_assert_report(pressed_report);

  event_keyboard_test_press_key(IKC_A);
  event_keyboard_test_assert_no_event();

  event_keyboard_test_release_key(IKC_A);
  event_keyboard_test_assert_report(released_report);

  event_keyboard_test_release_key(IKC_A);
  event_keyboard_test_assert_no_event();
}

static void test_event_keyboard_six_key_rollover(void) {
  const uint8_t six_key_report[HID_KEYBOARD_REPORT_SIZE] = {
      0,
      0,
      (uint8_t)ICODE_OPERAND(IKC_A),
      (uint8_t)ICODE_OPERAND(IKC_B),
      (uint8_t)ICODE_OPERAND(IKC_C),
      (uint8_t)ICODE_OPERAND(IKC_D),
      (uint8_t)ICODE_OPERAND(IKC_E),
      (uint8_t)ICODE_OPERAND(IKC_F),
  };
  const uint8_t after_release_report[HID_KEYBOARD_REPORT_SIZE] = {
      0,
      0,
      (uint8_t)ICODE_OPERAND(IKC_A),
      (uint8_t)ICODE_OPERAND(IKC_B),
      (uint8_t)ICODE_OPERAND(IKC_D),
      (uint8_t)ICODE_OPERAND(IKC_E),
      (uint8_t)ICODE_OPERAND(IKC_F),
      0,
  };
  const uint8_t after_replacement_report[HID_KEYBOARD_REPORT_SIZE] = {
      0,
      0,
      (uint8_t)ICODE_OPERAND(IKC_A),
      (uint8_t)ICODE_OPERAND(IKC_B),
      (uint8_t)ICODE_OPERAND(IKC_D),
      (uint8_t)ICODE_OPERAND(IKC_E),
      (uint8_t)ICODE_OPERAND(IKC_F),
      (uint8_t)ICODE_OPERAND(IKC_G),
  };

  event_keyboard_test_prepare(IKC_A);

  event_keyboard_test_press_key(IKC_A);
  event_keyboard_test_assert_report(
      (uint8_t[]){0, 0, (uint8_t)ICODE_OPERAND(IKC_A), 0, 0, 0, 0, 0});
  event_keyboard_test_press_key(IKC_B);
  event_keyboard_test_assert_report(
      (uint8_t[]){0, 0, (uint8_t)ICODE_OPERAND(IKC_A),
                  (uint8_t)ICODE_OPERAND(IKC_B), 0, 0, 0, 0});
  event_keyboard_test_press_key(IKC_C);
  event_keyboard_test_assert_report((uint8_t[]){
      0, 0, (uint8_t)ICODE_OPERAND(IKC_A), (uint8_t)ICODE_OPERAND(IKC_B),
      (uint8_t)ICODE_OPERAND(IKC_C), 0, 0, 0});
  event_keyboard_test_press_key(IKC_D);
  event_keyboard_test_assert_report((uint8_t[]){
      0, 0, (uint8_t)ICODE_OPERAND(IKC_A), (uint8_t)ICODE_OPERAND(IKC_B),
      (uint8_t)ICODE_OPERAND(IKC_C), (uint8_t)ICODE_OPERAND(IKC_D), 0, 0});
  event_keyboard_test_press_key(IKC_E);
  event_keyboard_test_assert_report((uint8_t[]){
      0, 0, (uint8_t)ICODE_OPERAND(IKC_A), (uint8_t)ICODE_OPERAND(IKC_B),
      (uint8_t)ICODE_OPERAND(IKC_C), (uint8_t)ICODE_OPERAND(IKC_D),
      (uint8_t)ICODE_OPERAND(IKC_E), 0});
  event_keyboard_test_press_key(IKC_F);
  event_keyboard_test_assert_report(six_key_report);

  event_keyboard_test_press_key(IKC_G);
  event_keyboard_test_assert_no_event();

  event_keyboard_test_release_key(IKC_C);
  event_keyboard_test_assert_report(after_release_report);

  event_keyboard_test_press_key(IKC_G);
  event_keyboard_test_assert_report(after_replacement_report);
}

static void test_event_keyboard_combined_virtual_modifiers(void) {
  const uint8_t first_pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      KMC_LEFT_SHIFT, 0, (uint8_t)ICODE_OPERAND(IKC_A), 0, 0, 0, 0, 0,
  };
  const uint8_t both_pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      KMC_LEFT_CONTROL | KMC_LEFT_SHIFT,
      0,
      (uint8_t)ICODE_OPERAND(IKC_A),
      (uint8_t)ICODE_OPERAND(IKC_B),
      0,
      0,
      0,
      0,
  };
  const uint8_t first_released_report[HID_KEYBOARD_REPORT_SIZE] = {
      KMC_LEFT_SHIFT, 0, (uint8_t)ICODE_OPERAND(IKC_A), 0, 0, 0, 0, 0,
  };
  const uint8_t released_report[HID_KEYBOARD_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(LEFT_SHIFT(IKC_A));

  event_keyboard_test_press_key(LEFT_SHIFT(IKC_A));
  event_keyboard_test_assert_report(first_pressed_report);

  event_keyboard_test_press_key(LEFT_CTRL(IKC_B));
  event_keyboard_test_assert_report(both_pressed_report);

  event_keyboard_test_release_key(LEFT_CTRL(IKC_B));
  event_keyboard_test_assert_report(first_released_report);

  event_keyboard_test_release_key(LEFT_SHIFT(IKC_A));
  event_keyboard_test_assert_report(released_report);
}

static void test_event_keyboard_direct_modifier_and_virtual_modifier(void) {
  const uint8_t modifier_report[HID_KEYBOARD_REPORT_SIZE] = {
      KMC_LEFT_SHIFT, 0, 0, 0, 0, 0, 0, 0,
  };
  const uint8_t modified_key_report[HID_KEYBOARD_REPORT_SIZE] = {
      KMC_LEFT_SHIFT, 0, (uint8_t)ICODE_OPERAND(IKC_A), 0, 0, 0, 0, 0,
  };
  const uint8_t released_report[HID_KEYBOARD_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(IKC_NOOP);

  event_keyboard_test_press_key(IMKC_LEFT_SHIFT);
  event_keyboard_test_assert_report(modifier_report);

  event_keyboard_test_press_key(LEFT_SHIFT(IKC_A));
  event_keyboard_test_assert_report(modified_key_report);

  event_keyboard_test_release_key(IMKC_LEFT_SHIFT);
  event_keyboard_test_assert_no_event();

  event_keyboard_test_release_key(LEFT_SHIFT(IKC_A));
  event_keyboard_test_assert_report(released_report);
}

static void test_event_keyboard_ignores_noop(void) {
  event_keyboard_test_prepare(IKC_NOOP);

  event_keyboard_test_press_key(IKC_NOOP);
  event_keyboard_test_assert_no_event();

  event_keyboard_test_release_key(IKC_NOOP);
  event_keyboard_test_assert_no_event();
}

static void test_event_keyboard_consumer_key_press_and_release(void) {
  const uint8_t volume_up_report[HID_CONSUMER_REPORT_SIZE] = {
      (uint8_t)CC_VOL_UP,
      (uint8_t)(CC_VOL_UP >> 8),
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
  };
  const uint8_t both_pressed_report[HID_CONSUMER_REPORT_SIZE] = {
      (uint8_t)CC_VOL_UP,
      (uint8_t)(CC_VOL_UP >> 8),
      (uint8_t)CC_VOL_DOWN,
      (uint8_t)(CC_VOL_DOWN >> 8),
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
  };
  const uint8_t volume_down_report[HID_CONSUMER_REPORT_SIZE] = {
      (uint8_t)CC_VOL_DOWN,
      (uint8_t)(CC_VOL_DOWN >> 8),
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
      0,
  };
  const uint8_t released_report[HID_CONSUMER_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(ICC_VOL_UP);

  event_keyboard_test_press_key(ICC_VOL_UP);
  event_keyboard_test_assert_consumer_report(volume_up_report);

  event_keyboard_test_press_key(ICC_VOL_DOWN);
  event_keyboard_test_assert_consumer_report(both_pressed_report);

  event_keyboard_test_release_key(ICC_VOL_UP);
  event_keyboard_test_assert_consumer_report(volume_down_report);

  event_keyboard_test_release_key(ICC_VOL_DOWN);
  event_keyboard_test_assert_consumer_report(released_report);
  event_keyboard_test_assert_no_event();
}

static void test_event_keyboard_special_key_operations(void) {
  const icode_t special_keycodes[] = {
      ISC_BOOT,       ISC_CONN_TOGGLE, ISC_CONN_USB,
      ISC_CONN_BLE,   ISC_BLE_UNPAIR,  ISC_BLE_SLOT_1,
      ISC_BLE_SLOT_2, ISC_BLE_SLOT_3,  ISC_BLE_SLOT_4,
  };

  for (uint8_t index = 0;
       index < sizeof(special_keycodes) / sizeof(special_keycodes[0]);
       index++) {
    event_keyboard_test_prepare(special_keycodes[index]);

    event_keyboard_test_press_key(special_keycodes[index]);
    event_keyboard_test_assert_platform_event(special_keycodes[index], true);

    event_keyboard_test_release_key(special_keycodes[index]);
    event_keyboard_test_assert_platform_event(special_keycodes[index], false);
  }
}

static void test_event_keyboard_mouse_key_buttons(void) {
  const uint8_t left_pressed_report[HID_MOUSE_REPORT_SIZE] = {MBC_LEFT, 0, 0,
                                                              0};
  const uint8_t both_pressed_report[HID_MOUSE_REPORT_SIZE] = {
      MBC_LEFT | MBC_RIGHT,
      0,
      0,
      0,
  };
  const uint8_t right_pressed_report[HID_MOUSE_REPORT_SIZE] = {MBC_RIGHT, 0, 0,
                                                               0};
  const uint8_t released_report[HID_MOUSE_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(IMC_MOUSE_LEFT);

  event_keyboard_test_press_key(IMC_MOUSE_LEFT);
  event_keyboard_test_assert_mouse_report(left_pressed_report);

  event_keyboard_test_press_key(IMC_MOUSE_RIGHT);
  event_keyboard_test_assert_mouse_report(both_pressed_report);

  event_keyboard_test_release_key(IMC_MOUSE_LEFT);
  event_keyboard_test_assert_mouse_report(right_pressed_report);

  event_keyboard_test_release_key(IMC_MOUSE_RIGHT);
  event_keyboard_test_assert_mouse_report(released_report);
}

static void test_event_keyboard_mouse_key_move_and_wheel(void) {
  const uint8_t move_up_report[HID_MOUSE_REPORT_SIZE] = {0, 0, 0xFF, 0};
  const uint8_t move_up_right_report[HID_MOUSE_REPORT_SIZE] = {0, 1, 0xFF, 0};
  const uint8_t move_right_report[HID_MOUSE_REPORT_SIZE] = {0, 1, 0, 0};
  const uint8_t wheel_up_report[HID_MOUSE_REPORT_SIZE] = {0, 0, 0, 1};
  const uint8_t wheel_down_report[HID_MOUSE_REPORT_SIZE] = {0, 0, 0, 0xFF};

  event_keyboard_test_prepare(IMC_MOUSE_MOVE_UP);

  event_keyboard_test_press_key(IMC_MOUSE_MOVE_UP);
  event_keyboard_test_assert_no_event();
  event_process_periodic();
  event_keyboard_test_assert_mouse_report(move_up_report);

  event_keyboard_test_press_key(IMC_MOUSE_MOVE_RIGHT);
  event_process_periodic();
  event_keyboard_test_assert_mouse_report(move_up_right_report);

  event_keyboard_test_release_key(IMC_MOUSE_MOVE_UP);
  event_process_periodic();
  event_keyboard_test_assert_mouse_report(move_right_report);

  event_keyboard_test_release_key(IMC_MOUSE_MOVE_RIGHT);
  event_process_periodic();
  event_keyboard_test_assert_no_event();

  event_keyboard_test_press_key(IMC_MOUSE_WHEEL_UP);
  event_process_periodic();
  event_keyboard_test_assert_mouse_report(wheel_up_report);

  event_keyboard_test_release_key(IMC_MOUSE_WHEEL_UP);
  event_process_periodic();
  event_keyboard_test_assert_no_event();

  event_keyboard_test_press_key(IMC_MOUSE_WHEEL_DOWN);
  event_process_periodic();
  event_keyboard_test_assert_mouse_report(wheel_down_report);
}

static void test_event_keyboard_pointing_device_operations(void) {
  const uint8_t combined_report[HID_MOUSE_REPORT_SIZE] = {
      MBC_LEFT | MBC_RIGHT,
      1,
      1,
      0xFF,
  };
  const uint8_t remaining_button_report[HID_MOUSE_REPORT_SIZE] = {
      MBC_LEFT,
      0,
      0,
      0,
  };
  const uint8_t released_report[HID_MOUSE_REPORT_SIZE] = {0};

  event_keyboard_test_prepare(IKC_NOOP);

  int8_t pointing_device_id = event_request_pointing_device_id();
  TEST_ASSERT_EQUAL_INT8(1, pointing_device_id);

  event_accumulate_mouse(0, MBC_LEFT, 2, -3, 1);
  event_accumulate_mouse((uint8_t)pointing_device_id, MBC_RIGHT, -1, 4, -2);
  event_keyboard_test_assert_mouse_report(combined_report);

  event_accumulate_mouse((uint8_t)pointing_device_id, MBC_UNDEFINED, 0, 0, 0);
  event_keyboard_test_assert_mouse_report(remaining_button_report);

  event_accumulate_mouse(0, MBC_UNDEFINED, 0, 0, 0);
  event_keyboard_test_assert_mouse_report(released_report);
}

int main(void) {
  UNITY_BEGIN();

  // テストケース: キーの押下と解放
  RUN_TEST(test_event_keyboard_key_press_and_release);
  // テストケース: モディファイア付きのキー押下
  RUN_TEST(test_event_keyboard_key_with_modifier);
  // テストケース: 複数キーの押下と解放順序
  RUN_TEST(test_event_keyboard_multiple_keys_and_release_order);
  // テストケース: 重複したキーの押下と解放
  RUN_TEST(test_event_keyboard_duplicate_press_and_release);
  // テストケース: 6キーのロールオーバー
  RUN_TEST(test_event_keyboard_six_key_rollover);
  // テストケース: 仮想モディファイアの組み合わせ
  RUN_TEST(test_event_keyboard_combined_virtual_modifiers);
  // テストケース: 直接モディファイアと仮想モディファイアの組み合わせ
  RUN_TEST(test_event_keyboard_direct_modifier_and_virtual_modifier);
  // テストケース: IKC_NOOP の無視
  RUN_TEST(test_event_keyboard_ignores_noop);
  // テストケース: コンシューマーキーの押下と解放
  RUN_TEST(test_event_keyboard_consumer_key_press_and_release);
  // テストケース: 特殊キーのプラットフォーム処理
  RUN_TEST(test_event_keyboard_special_key_operations);
  // テストケース: マウスキーのボタン操作
  RUN_TEST(test_event_keyboard_mouse_key_buttons);
  // テストケース: マウスキーの移動とホイール操作
  RUN_TEST(test_event_keyboard_mouse_key_move_and_wheel);
  // テストケース: 複数ポインティングデバイスの操作
  RUN_TEST(test_event_keyboard_pointing_device_operations);

  return UNITY_END();
}
