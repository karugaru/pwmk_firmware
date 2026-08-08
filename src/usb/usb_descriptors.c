#if PWMK_ENABLE_USB

#include <pico/unique_id.h>
#include <stddef.h>
#include <string.h>
#include <tusb.h>

#include "device_identity.h"
#include "hid/hid.h"
#include "settings/settings.h"
#include "usb/usb_descriptors.h"
#include "vial/vial.h"

#define VIAL_SERIAL_PREFIX "vial:f64c2b3c-"

// デバイスディスクリプタ
static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&desc_device;
}

static uint8_t const vial_hid_descriptor[] = {
    0x06, 0x60, 0xFF, // Usage Page (Vendor Defined 0xFF60)
    0x09, 0x61,       // Usage (0x61)
    0xA1, 0x01,       // Collection (Application)

    0x09, 0x62,       // Usage (0x62)
    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xFF, 0x00, // Logical Maximum (255)
    0x75, 0x08,       // Report Size (8)
    0x95, VIAL_PACKET_SIZE, // Report Count (32)
    0x81, 0x02,       // Input (Data, Variable, Absolute)

    0x09, 0x63,       // Usage (0x63)
    0x95, VIAL_PACKET_SIZE, // Report Count (32)
    0x91, 0x02,       // Output (Data, Variable, Absolute)

    0xC0, // End collection
};

// HIDレポートディスクリプタ
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  return instance == USB_HID_INSTANCE_VIAL ? vial_hid_descriptor
                                            : hid_descriptor;
}

enum { ITF_NUM_HID, ITF_NUM_VIAL, ITF_NUM_TOTAL };

#define CONFIG_TOTAL_LEN                                                        \
  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_INOUT_DESC_LEN)
#define EPNUM_HID 0x81
#define EPNUM_VIAL_OUT 0x02
#define EPNUM_VIAL_IN 0x82

// コンフィグレーションディスクリプタ
// HIDレポートディスクリプタのサイズはコンパイル時に不明のため、
// 実行時にコンフィグレーションディスクリプタのサイズフィールドを修正する
static uint8_t desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // HIDインターフェース。
    // レポートディスクリプタの長さは初期化時に修正される。
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, 0, EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE, 1),

    // Vial専用Raw HIDインターフェース。Report IDなしで32バイトを送受信する。
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_VIAL, 0, HID_ITF_PROTOCOL_NONE, 0,
                 EPNUM_VIAL_OUT, EPNUM_VIAL_IN, VIAL_PACKET_SIZE,
                 1),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t _index) {
  return desc_configuration;
}

enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
};

// 文字列ディスクリプタ
static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: English (0x0409)
    MANUFACTURER_NAME,          // 1: Manufacturer
    DEVICE_NAME,                // 2: Product
    NULL,                       // 3: Serial (pico_unique_idで自動生成)
};

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void)langid;
  size_t chr_count;
  size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;

  switch (index) {
  case STRID_LANGID:
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
    break;

  case STRID_SERIAL: {
    // Vial GUIの自動検出識別子を先頭に付与し、Pico固有IDで一意性を保つ。
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    chr_count = 0;
    for (size_t i = 0;
         i < sizeof(VIAL_SERIAL_PREFIX) - 1 && chr_count < max_count; i++) {
      _desc_str[1 + chr_count++] = VIAL_SERIAL_PREFIX[i];
    }
    for (size_t i = 0;
         i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES && chr_count + 1 < max_count;
         i++) {
      uint8_t byte = board_id.id[i];
      uint8_t hi = (byte >> 4) & 0x0F;
      uint8_t lo = byte & 0x0F;
      _desc_str[1 + chr_count++] = hi < 10 ? ('0' + hi) : ('A' + hi - 10);
      _desc_str[1 + chr_count++] = lo < 10 ? ('0' + lo) : ('A' + lo - 10);
    }
    break;
  }

  default:
    if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
      return NULL;
    }

    const char *str = string_desc_arr[index];
    chr_count = strlen(str);
    if (chr_count > max_count) {
      chr_count = max_count;
    }

    // ASCII → UTF-16変換
    for (size_t i = 0; i < chr_count; i++) {
      _desc_str[1 + i] = str[i];
    }
    break;
  }

  // 先頭バイト: 長さ(ヘッダ含む)、2バイト目: 文字列型
  _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}

void usb_descriptors_init(void) {
  // 各 HID descriptor 内の wReportLength フィールドを設定する。
  const size_t report_length_offset =
      sizeof(tusb_desc_interface_t) +
      offsetof(tusb_hid_descriptor_hid_t, wReportLength);
  const size_t input_offset = TUD_CONFIG_DESC_LEN + report_length_offset;
  const size_t vial_offset =
      TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + report_length_offset;
  uint16_t input_report_length = hid_descriptor_len;
  uint16_t vial_report_length = sizeof(vial_hid_descriptor);

  desc_configuration[input_offset] = input_report_length & 0xFF;
  desc_configuration[input_offset + 1] = input_report_length >> 8;
  desc_configuration[vial_offset] = vial_report_length & 0xFF;
  desc_configuration[vial_offset + 1] = vial_report_length >> 8;
}

#endif // PWMK_ENABLE_USB
