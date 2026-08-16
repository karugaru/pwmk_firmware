#include "profile/keymap.h"
#include "unity.h"

const int8_t layout[ROWS * COLS][2] = {
    {0, 0},
    {0, 1},
    {1, 0},
    {-1, -1},
};

static icode_t dynamic_keymap[KEYMAP_ENTRY_COUNT];

void setUp(void) {
  keymap_init();
  keymap_reset(dynamic_keymap, KEYMAP_ENTRY_COUNT);
}

void tearDown(void) {}

static void test_keymap_resets_caller_owned_state(void) {
  TEST_ASSERT_EQUAL_UINT32(IKC_A, keymap_get(dynamic_keymap, 0, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(LEFT_SHIFT(IKC_B),
                           keymap_get(dynamic_keymap, 0, 0, 1));
  TEST_ASSERT_EQUAL_UINT32(ICC_VOL_UP, keymap_get(dynamic_keymap, 0, 1, 0));
  TEST_ASSERT_EQUAL_UINT32(IKC_NOOP, keymap_get(dynamic_keymap, 0, 1, 1));
}

static void test_keymap_validates_position_and_updates_state(void) {
  TEST_ASSERT_TRUE(keymap_is_valid_position(0, 0, 0));
  TEST_ASSERT_FALSE(keymap_is_valid_position(0, 1, 1));
  TEST_ASSERT_TRUE(keymap_set(dynamic_keymap, 0, 0, 0, IKC_C));
  TEST_ASSERT_EQUAL_UINT32(IKC_C, keymap_get(dynamic_keymap, 0, 0, 0));
  TEST_ASSERT_FALSE(keymap_set(dynamic_keymap, 0, 1, 1, IKC_C));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_keymap_resets_caller_owned_state);
  RUN_TEST(test_keymap_validates_position_and_updates_state);
  return UNITY_END();
}
