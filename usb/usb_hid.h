#ifndef PWMK_USB_HID_H
#define PWMK_USB_HID_H

#include <stdbool.h>

void usb_hid_init(void);
void usb_hid_task(void);
void usb_hid_send_reports(void);
bool usb_hid_is_active(void);

#endif // PWMK_USB_HID_H
