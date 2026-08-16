#include <string.h>

#include "profile/keymap.h"
#include "settings/settings.h"
#include "unity.h"

const int8_t layout[ROWS * COLS][2] = {
    {0, 0},
    {0, 1},
    {1, 0},
    {-1, -1},
};

static bool persistence_init_result;
static bool persistence_restore_result;
static bool persistence_rebuild_result;
static bool persistence_commit_result;
static size_t persistence_image_size;
static size_t persistence_restore_calls;
static size_t persistence_rebuild_calls;
static size_t persistence_commit_calls;
static uint8_t persisted_image[128];
static bool persistence_restore_partial_failure;

bool persistence_init(size_t image_size) {
  persistence_image_size = image_size;
  return persistence_init_result;
}

bool persistence_restore(uint8_t *image, size_t image_size) {
  persistence_restore_calls++;
  if (!persistence_restore_result) {
    if (persistence_restore_partial_failure && image != NULL) {
      memset(image, 0xFF, image_size);
    }
    return false;
  }
  if (image == NULL || image_size != persistence_image_size) {
    return false;
  }
  memcpy(image, persisted_image, image_size);
  return true;
}

bool persistence_commit(const uint8_t *image, size_t image_size) {
  persistence_commit_calls++;
  if (!persistence_commit_result || image == NULL ||
      image_size != persistence_image_size) {
    return false;
  }
  memcpy(persisted_image, image, image_size);
  return true;
}

bool persistence_rebuild(const uint8_t *image, size_t image_size) {
  persistence_rebuild_calls++;
  if (!persistence_rebuild_result || image == NULL ||
      image_size != persistence_image_size) {
    return false;
  }
  memcpy(persisted_image, image, image_size);
  return true;
}

void setUp(void) {
  persistence_init_result = true;
  persistence_restore_result = false;
  persistence_rebuild_result = true;
  persistence_commit_result = true;
  persistence_image_size = 0;
  persistence_restore_calls = 0;
  persistence_rebuild_calls = 0;
  persistence_commit_calls = 0;
  persistence_restore_partial_failure = false;
  memset(persisted_image, 0, sizeof(persisted_image));
}

void tearDown(void) {}

static void test_settings_initializes_defaults_and_persists_change(void) {
  settings_init();

  TEST_ASSERT_EQUAL_UINT32(IKC_A, settings_get_keycode(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT(sizeof(icode_t) * KEYMAP_ENTRY_COUNT,
                         settings_image_size());
  TEST_ASSERT_EQUAL_UINT(1, persistence_rebuild_calls);

  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_PERSISTED,
                        settings_set_keycode(0, 0, 0, IKC_C));
  TEST_ASSERT_EQUAL_UINT32(IKC_C, settings_get_keycode(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT(1, persistence_commit_calls);
}

static void test_settings_keeps_ram_change_after_commit_failure(void) {
  settings_init();
  persistence_commit_result = false;

  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_RAM_ONLY,
                        settings_set_keycode(0, 0, 0, IKC_C));
  TEST_ASSERT_EQUAL_UINT32(IKC_C, settings_get_keycode(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT(1, persistence_commit_calls);

  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_RAM_ONLY,
                        settings_set_keycode(0, 0, 1, IKC_D));
  TEST_ASSERT_EQUAL_UINT32(IKC_D, settings_get_keycode(0, 0, 1));
  TEST_ASSERT_EQUAL_UINT(1, persistence_commit_calls);

  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_RAM_ONLY,
                        settings_set_keycode(0, 0, 1, IKC_D));
}

static void test_settings_rejects_invalid_input_without_changes(void) {
  settings_init();

  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_FAILED,
                        settings_set_keycode(0, 1, 1, IKC_C));
  TEST_ASSERT_EQUAL_UINT32(IKC_NOOP, settings_get_keycode(0, 1, 1));
  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_FAILED,
                        settings_set_keycode(0, 0, 0, (icode_t)UINT32_MAX));
  TEST_ASSERT_EQUAL_UINT32(IKC_A, settings_get_keycode(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT(0, persistence_commit_calls);
}

static void test_settings_restores_without_rebuilding(void) {
  settings_init();
  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_PERSISTED,
                        settings_set_keycode(0, 0, 0, IKC_C));

  persistence_restore_result = true;
  persistence_rebuild_result = false;
  persistence_restore_calls = 0;
  persistence_rebuild_calls = 0;
  settings_init();

  TEST_ASSERT_EQUAL_UINT32(IKC_C, settings_get_keycode(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT(1, persistence_restore_calls);
  TEST_ASSERT_EQUAL_UINT(0, persistence_rebuild_calls);
}

static void test_settings_resets_defaults_after_partial_restore_failure(void) {
  persistence_restore_partial_failure = true;

  settings_init();

  TEST_ASSERT_EQUAL_UINT32(IKC_A, settings_get_keycode(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT(1, persistence_rebuild_calls);
}

static void
test_settings_accepts_ram_changes_after_persistence_init_failure(void) {
  persistence_init_result = false;
  settings_init();

  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_RAM_ONLY,
                        settings_set_keycode(0, 0, 0, IKC_C));
  TEST_ASSERT_EQUAL_UINT32(IKC_C, settings_get_keycode(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT(0, persistence_commit_calls);
}

static void test_settings_reset_keeps_ram_result_when_rebuild_fails(void) {
  settings_init();
  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_PERSISTED,
                        settings_set_keycode(0, 0, 0, IKC_C));

  persistence_rebuild_result = false;
  TEST_ASSERT_EQUAL_INT(SETTINGS_UPDATE_RAM_ONLY,
                        settings_reset(SETTINGS_ID_KEYMAP));
  TEST_ASSERT_EQUAL_UINT32(IKC_A, settings_get_keycode(0, 0, 0));
  TEST_ASSERT_EQUAL_UINT(2, persistence_rebuild_calls);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_settings_initializes_defaults_and_persists_change);
  RUN_TEST(test_settings_keeps_ram_change_after_commit_failure);
  RUN_TEST(test_settings_rejects_invalid_input_without_changes);
  RUN_TEST(test_settings_restores_without_rebuilding);
  RUN_TEST(test_settings_resets_defaults_after_partial_restore_failure);
  RUN_TEST(test_settings_accepts_ram_changes_after_persistence_init_failure);
  RUN_TEST(test_settings_reset_keeps_ram_result_when_rebuild_fails);
  return UNITY_END();
}
