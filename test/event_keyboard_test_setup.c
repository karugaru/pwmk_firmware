#include "event_keyboard_test_setup.h"
#include "keyboard/event.h"

static icode_t test_keycode = IKC_NOOP;
static bool platform_event_pending = false;
static icode_t platform_event_keycode = IKC_NOOP;
static bool platform_event_pressed = false;

static icode_t event_keyboard_test_keymap_get(uint8_t layer, uint8_t row,
                                              uint8_t col) {
  (void)layer;
  (void)row;
  (void)col;
  return test_keycode;
}

static bool event_keyboard_test_platform_callback(icode_t keycode,
                                                  bool pressed) {
  if (ICODE_OPCODE(keycode) != ICODE_OPCODE_SYSTEM) {
    return false;
  }

  platform_event_pending = true;
  platform_event_keycode = keycode;
  platform_event_pressed = pressed;
  return true;
}

void event_keyboard_test_prepare(icode_t keycode) {
  test_keycode = keycode;
  platform_event_pending = false;
  event_init((event_settings_t){
      .mouse_move_thresh = 1,
      .mouse_wheel_thresh = 1,
      .mouse_move_delta = 1,
      .mouse_wheel_delta = 1,
      .platform_callback = event_keyboard_test_platform_callback,
      .keymap_get_callback = event_keyboard_test_keymap_get,
  });
}

void event_keyboard_test_set_keycode(icode_t keycode) {
  test_keycode = keycode;
}

bool event_keyboard_test_pop_platform_event(icode_t *keycode, bool *pressed) {
  if (!platform_event_pending) {
    return false;
  }

  *keycode = platform_event_keycode;
  *pressed = platform_event_pressed;
  platform_event_pending = false;
  return true;
}
