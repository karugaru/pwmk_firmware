#ifndef PWMK_EVENT_H
#define PWMK_EVENT_H

#include "hid/hid.h"
#include "keyboard/code.h"

typedef struct {
  uint8_t report_id;
  size_t size;
  uint8_t data[HID_REPORT_SIZE_MAX];
} keymap_hid_report_t;

void event_init(void);
int8_t event_request_pointing_device_id(void);

void event_accumulate_mouse(uint8_t device_id, mouse_button_code_t buttons,
                            int8_t x, int8_t y, int8_t w);
void event_process(icode_t icode, bool pressed);
void event_process_periodic(void);

bool event_has_event(void);
bool event_pop_hid_report(keymap_hid_report_t *report);

bool event_process_user_cb(icode_t *icode, bool pressed);

#endif // PWMK_EVENT_H
