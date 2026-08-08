#ifndef PWMK_EVENT_H
#define PWMK_EVENT_H

#include "hid/hid.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t report_id;
  uint16_t size;
  uint8_t data[HID_REPORT_SIZE_MAX];
} keymap_hid_report_t;

typedef bool (*event_platform_cb_t)(icode_t icode, bool pressed);

typedef struct {
  int16_t mouse_move_thresh;
  int16_t mouse_wheel_thresh;
  int16_t mouse_move_delta;
  int16_t mouse_wheel_delta;
  event_platform_cb_t platform_callback;
} event_settings_t;

void event_init(event_settings_t settings);

int8_t event_request_pointing_device_id(void);

void event_accumulate_mouse(uint8_t device_id, mouse_button_code_t buttons,
                            int8_t x, int8_t y, int8_t w);
void event_process(icode_t icode, bool pressed, uint64_t event_time);
void event_process_periodic(void);

bool event_has_event(void);
bool event_pop_hid_report(keymap_hid_report_t *report);

bool event_process_user_cb(icode_t *icode, bool pressed, uint64_t event_time);

#endif // PWMK_EVENT_H
