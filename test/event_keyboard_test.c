#include "event_keyboard_test_setup.h"
#include "keyboard/event.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_event_keyboard_key_press_and_release(void) {
  const uint8_t pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      0, 0, IKC_A, 0, 0, 0, 0, 0,
  };
  const uint8_t released_report[HID_KEYBOARD_REPORT_SIZE] = {0};
  event_hid_report_t report;

  event_keyboard_test_prepare(IKC_A);

  event_process(0, 0, true, 0);
  TEST_ASSERT_TRUE(event_has_event());
  TEST_ASSERT_TRUE(event_pop_hid_report(&report));
  TEST_ASSERT_EQUAL_UINT8(HID_KEYBOARD_REPORT_ID, report.report_id);
  TEST_ASSERT_EQUAL_UINT8(HID_KEYBOARD_REPORT_SIZE, report.size);
  TEST_ASSERT_EQUAL_MEMORY(pressed_report, report.data, sizeof(pressed_report));
  TEST_ASSERT_FALSE(event_has_event());

  event_process(0, 0, false, 0);
  TEST_ASSERT_TRUE(event_pop_hid_report(&report));
  TEST_ASSERT_EQUAL_MEMORY(released_report, report.data, sizeof(released_report));
  TEST_ASSERT_FALSE(event_has_event());
}

static void test_event_keyboard_key_with_modifier(void) {
  const uint8_t pressed_report[HID_KEYBOARD_REPORT_SIZE] = {
      KMC_LEFT_SHIFT, 0, IKC_A, 0, 0, 0, 0, 0,
  };
  const uint8_t released_report[HID_KEYBOARD_REPORT_SIZE] = {0};
  event_hid_report_t report;

  event_keyboard_test_prepare(LEFT_SHIFT(IKC_A));

  event_process(0, 0, true, 0);
  TEST_ASSERT_TRUE(event_pop_hid_report(&report));
  TEST_ASSERT_EQUAL_UINT8(HID_KEYBOARD_REPORT_ID, report.report_id);
  TEST_ASSERT_EQUAL_MEMORY(pressed_report, report.data, sizeof(pressed_report));

  event_process(0, 0, false, 0);
  TEST_ASSERT_TRUE(event_pop_hid_report(&report));
  TEST_ASSERT_EQUAL_MEMORY(released_report, report.data, sizeof(released_report));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_event_keyboard_key_press_and_release);
  RUN_TEST(test_event_keyboard_key_with_modifier);
  return UNITY_END();
}
