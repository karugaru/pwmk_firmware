#ifndef PWMK_USB_DESCRIPTORS_H
#define PWMK_USB_DESCRIPTORS_H

#define USB_BCD 0x0200
#define USB_HID_INSTANCE_INPUT 0
#define USB_HID_INSTANCE_VIAL 1

/**
 * @brief
 * USBをHID機器として使用する際のコンフィグレーションディスクリプタを初期化する。
 */
void usb_descriptors_init(void);

#endif // PWMK_USB_DESCRIPTORS_H
