#include "usb/usb_hid.h"

/*
 * 公開関数
 */

__attribute__((weak)) void usb_hid_init(void) {}

__attribute__((weak)) void usb_hid_deinit(void) {}

__attribute__((weak)) void usb_hid_task(void) {}

__attribute__((weak)) void usb_hid_send_reports(void) {}

__attribute__((weak)) bool usb_hid_is_active(void) { return false; }
