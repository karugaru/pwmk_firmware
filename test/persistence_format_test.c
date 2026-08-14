#include <string.h>

#include "settings/persistence_format.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_progress_accepts_valid_sequences(void) {
  uint8_t progress[PWMK_PROGRESS_SIZE];
  pwmk_progress_info_t info;
  size_t slot;

  memset(progress, 0xFF, sizeof(progress));
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_EMPTY, info.state);
  TEST_ASSERT_EQUAL_UINT(0, info.completed_save_count);

  progress[0] = 0x3F;
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_COMPLETE, info.state);
  TEST_ASSERT_EQUAL_UINT(1, info.completed_save_count);
  TEST_ASSERT_TRUE(persistence_format_progress_get_next_slot(&info, &slot));
  TEST_ASSERT_EQUAL_UINT(1, slot);

  progress[0] = 0x0F;
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_COMPLETE, info.state);
  TEST_ASSERT_EQUAL_UINT(2, info.completed_save_count);

  progress[0] = 0x7F;
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_IN_PROGRESS, info.state);
  TEST_ASSERT_EQUAL_UINT(0, info.completed_save_count);
}

static void test_progress_rejects_invalid_sequences(void) {
  uint8_t progress[PWMK_PROGRESS_SIZE];
  pwmk_progress_info_t info;

  memset(progress, 0xFF, sizeof(progress));
  progress[0] = 0x1F;
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_IN_PROGRESS, info.state);

  memset(progress, 0xFF, sizeof(progress));
  progress[0] = 0x6F;
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_DIRTY, info.state);

  memset(progress, 0xFF, sizeof(progress));
  progress[0] = 0x33;
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_DIRTY, info.state);

  memset(progress, 0xFF, sizeof(progress));
  progress[0] = 0x13;
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_DIRTY, info.state);
}

static void test_progress_reports_full_capacity_and_bit_order(void) {
  uint8_t progress[PWMK_PROGRESS_SIZE];
  pwmk_progress_info_t info;
  size_t byte_offset;
  uint8_t clear_mask;

  memset(progress, 0x00, sizeof(progress));
  TEST_ASSERT_TRUE(
      persistence_format_progress_analyze(progress, sizeof(progress), &info));
  TEST_ASSERT_EQUAL_INT(PWMK_PROGRESS_COMPLETE, info.state);
  TEST_ASSERT_EQUAL_UINT(PWMK_PROGRESS_BIT_COUNT / 2,
                         info.completed_save_count);
  TEST_ASSERT_FALSE(
      persistence_format_progress_get_next_slot(&info, &byte_offset));

  TEST_ASSERT_TRUE(persistence_format_progress_get_slot_update(
      0, false, &byte_offset, &clear_mask));
  TEST_ASSERT_EQUAL_UINT(0, byte_offset);
  TEST_ASSERT_EQUAL_HEX8(0x80, clear_mask);
  TEST_ASSERT_TRUE(persistence_format_progress_get_slot_update(
      0, true, &byte_offset, &clear_mask));
  TEST_ASSERT_EQUAL_HEX8(0x40, clear_mask);
  TEST_ASSERT_TRUE(persistence_format_progress_get_slot_update(
      4, false, &byte_offset, &clear_mask));
  TEST_ASSERT_EQUAL_UINT(1, byte_offset);
  TEST_ASSERT_EQUAL_HEX8(0x80, clear_mask);
}

static void test_progress_updates_slots_in_commit_order(void) {
  uint8_t progress_byte = 0xFF;
  uint8_t updated_value;

  TEST_ASSERT_TRUE(persistence_format_progress_update_slot(
      progress_byte, 0, false, &updated_value));
  TEST_ASSERT_EQUAL_HEX8(0x7F, updated_value);
  progress_byte = updated_value;
  TEST_ASSERT_FALSE(persistence_format_progress_update_slot(
      progress_byte, 0, false, &updated_value));
  TEST_ASSERT_TRUE(persistence_format_progress_update_slot(
      progress_byte, 0, true, &updated_value));
  TEST_ASSERT_EQUAL_HEX8(0x3F, updated_value);
  progress_byte = updated_value;
  TEST_ASSERT_FALSE(persistence_format_progress_update_slot(
      progress_byte, 0, true, &updated_value));
  TEST_ASSERT_TRUE(persistence_format_progress_update_slot(
      progress_byte, 1, false, &updated_value));
  TEST_ASSERT_EQUAL_HEX8(0x1F, updated_value);
}

static void test_fixed_log_round_trip_pads_short_writes(void) {
  uint8_t built_data[16];
  uint8_t replay_data[16];
  uint8_t log[32];
  const uint8_t update[] = {0xAA, 0xBB};
  size_t encoded_size;
  size_t used_size;

  for (uint8_t index = 0; index < sizeof(built_data); index++) {
    built_data[index] = index;
  }
  memcpy(replay_data, built_data, sizeof(replay_data));
  memset(log, 0xFF, sizeof(log));

  TEST_ASSERT_TRUE(persistence_format_log_encode(
      log, sizeof(log), &encoded_size, built_data, sizeof(built_data), 8,
      update, sizeof(update)));
  TEST_ASSERT_EQUAL_UINT(PWMK_FIXED_LOG_RECORD_SIZE, encoded_size);
  TEST_ASSERT_EQUAL_HEX8(0x00, log[0]);
  TEST_ASSERT_EQUAL_HEX8(0x08, log[1]);
  TEST_ASSERT_EQUAL_HEX8(0xAA, log[2]);
  TEST_ASSERT_EQUAL_HEX8(0xBB, log[3]);
  TEST_ASSERT_EQUAL_HEX8(0x0A, log[4]);
  TEST_ASSERT_EQUAL_HEX8(0x0B, log[5]);

  TEST_ASSERT_TRUE(persistence_format_log_replay(
      log, sizeof(log), 1, replay_data, sizeof(replay_data), &used_size));
  TEST_ASSERT_EQUAL_UINT(PWMK_FIXED_LOG_RECORD_SIZE, used_size);
  TEST_ASSERT_EQUAL_HEX8(0xAA, replay_data[8]);
  TEST_ASSERT_EQUAL_HEX8(0xBB, replay_data[9]);
  TEST_ASSERT_EQUAL_HEX8(0x0A, replay_data[10]);
  TEST_ASSERT_EQUAL_HEX8(0x0B, replay_data[11]);
}

static void test_variable_log_round_trip_and_bounds(void) {
  uint8_t built_data[16] = {0};
  uint8_t replay_data[16] = {0};
  uint8_t log[32];
  const uint8_t update[] = {1, 2, 3, 4, 5};
  size_t encoded_size;

  memset(log, 0xFF, sizeof(log));
  TEST_ASSERT_TRUE(persistence_format_log_encode(
      log, sizeof(log), &encoded_size, built_data, sizeof(built_data), 2,
      update, sizeof(update)));
  TEST_ASSERT_EQUAL_UINT(PWMK_VARIABLE_LOG_HEADER_SIZE + sizeof(update),
                         encoded_size);
  TEST_ASSERT_EQUAL_HEX8(PWMK_VARIABLE_LOG_TYPE, log[0]);
  TEST_ASSERT_EQUAL_HEX8(sizeof(update), log[1]);
  TEST_ASSERT_TRUE(persistence_format_log_replay(
      log, sizeof(log), 1, replay_data, sizeof(replay_data), NULL));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(update, &replay_data[2], sizeof(update));

  memset(log, 0xFF, sizeof(log));
  log[0] = PWMK_VARIABLE_LOG_TYPE;
  log[1] = 0;
  TEST_ASSERT_FALSE(persistence_format_log_replay(
      log, sizeof(log), 1, replay_data, sizeof(replay_data), NULL));
}

static void test_log_replay_rejects_trailing_or_incomplete_data(void) {
  uint8_t built_data[16] = {0};
  uint8_t log[32];
  const uint8_t update[] = {0xA5};
  size_t encoded_size;

  memset(log, 0xFF, sizeof(log));
  TEST_ASSERT_TRUE(persistence_format_log_encode(
      log, sizeof(log), &encoded_size, built_data, sizeof(built_data), 0,
      update, sizeof(update)));
  log[encoded_size] = 0;
  TEST_ASSERT_FALSE(persistence_format_log_replay(
      log, sizeof(log), 1, built_data, sizeof(built_data), NULL));

  uint8_t incomplete_log[4] = {0, 0, 0xA5, 0xFF};
  TEST_ASSERT_FALSE(
      persistence_format_log_replay(incomplete_log, sizeof(incomplete_log), 1,
                                    built_data, sizeof(built_data), NULL));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_progress_accepts_valid_sequences);
  RUN_TEST(test_progress_rejects_invalid_sequences);
  RUN_TEST(test_progress_reports_full_capacity_and_bit_order);
  RUN_TEST(test_progress_updates_slots_in_commit_order);
  RUN_TEST(test_fixed_log_round_trip_pads_short_writes);
  RUN_TEST(test_variable_log_round_trip_and_bounds);
  RUN_TEST(test_log_replay_rejects_trailing_or_incomplete_data);
  return UNITY_END();
}
