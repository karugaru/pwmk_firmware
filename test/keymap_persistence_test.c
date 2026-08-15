#include <string.h>

#include "profile/keymap.h"
#include "unity.h"

const int8_t layout[ROWS * COLS][2] = {
    {0, 0},
    {0, 1},
    {1, 0},
    {-1, -1},
};

void setUp(void) { keymap_init(); }

void tearDown(void) {}

static void test_keymap_exports_pwmk_little_endian_data(void) {
  uint8_t data[PWMK_KEYMAP_DATA_SIZE];
  const uint8_t expected[] = {
      0x04, 0x00, 0x00, 0x00, 0x05, 0x02, 0x00, 0x00,
      0x0C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  TEST_ASSERT_TRUE(keymap_export_pwmk(data, sizeof(data)));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, data, sizeof(expected));
}

static void test_keymap_imports_pwmk_data_after_validation(void) {
  uint8_t data[PWMK_KEYMAP_DATA_SIZE];
  const uint8_t imported[] = {
      0x06, 0x00, 0x00, 0x00, 0x07, 0x02, 0x00, 0x00,
      0x0D, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  memcpy(data, imported, sizeof(data));
  TEST_ASSERT_TRUE(keymap_import_pwmk(data, sizeof(data)));
  TEST_ASSERT_EQUAL_UINT32(IKC_C, keymap_get(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(LEFT_SHIFT(IKC_D), keymap_get(0, 0, 1));
  TEST_ASSERT_EQUAL_UINT32(ICC_VOL_DOWN, keymap_get(0, 1, 0));

  data[0] = 0x01;
  TEST_ASSERT_FALSE(keymap_import_pwmk(data, sizeof(data)));
  TEST_ASSERT_EQUAL_UINT32(IKC_C, keymap_get(0, 0, 0));

  memcpy(data, imported, sizeof(data));
  data[PWMK_KEYCODE_SIZE * 3] = IKC_A;
  TEST_ASSERT_FALSE(keymap_import_pwmk(data, sizeof(data)));
  TEST_ASSERT_EQUAL_UINT32(IKC_C, keymap_get(0, 0, 0));
}

static void test_keymap_reports_layout_relative_offsets(void) {
  size_t offset;

  TEST_ASSERT_TRUE(keymap_get_pwmk_offset(0, 0, 0, &offset));
  TEST_ASSERT_EQUAL_UINT(0, offset);
  TEST_ASSERT_TRUE(keymap_get_pwmk_offset(0, 0, 1, &offset));
  TEST_ASSERT_EQUAL_UINT(PWMK_KEYCODE_SIZE, offset);
  TEST_ASSERT_TRUE(keymap_get_pwmk_offset(0, 1, 0, &offset));
  TEST_ASSERT_EQUAL_UINT(PWMK_KEYCODE_SIZE * 2, offset);
  TEST_ASSERT_FALSE(keymap_get_pwmk_offset(0, 1, 1, &offset));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_keymap_exports_pwmk_little_endian_data);
  RUN_TEST(test_keymap_imports_pwmk_data_after_validation);
  RUN_TEST(test_keymap_reports_layout_relative_offsets);
  return UNITY_END();
}
