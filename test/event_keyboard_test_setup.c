#include "event_keyboard_test_setup.h"
#include "keyboard/event.h"

static icode_t test_keycode = IKC_NOOP;

static icode_t event_keyboard_test_keymap_get(uint8_t layer, uint8_t row,
                                              uint8_t col) {
  (void)layer;
  (void)row;
  (void)col;
  return test_keycode;
}

void event_keyboard_test_prepare(icode_t keycode) {
  test_keycode = keycode;
  event_init((event_settings_t){
      .mouse_move_thresh = 1,
      .mouse_wheel_thresh = 1,
      .mouse_move_delta = 1,
      .mouse_wheel_delta = 1,
      .keymap_get_callback = event_keyboard_test_keymap_get,
  });
}
