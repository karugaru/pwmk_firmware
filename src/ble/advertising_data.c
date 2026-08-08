#if PWMK_ENABLE_BLE

#include <btstack.h>
#include <string.h>

#include "ble/advertising_data.h"
#include "device_identity.h"

uint8_t adv_data[64];
uint8_t adv_data_len = 0;

/**
 * @brief BLE Advertising用のデータを初期化する。
 */
void advertising_data_init(void) {
  size_t const name_len = strlen(DEVICE_NAME);
  uint16_t index = 0;

  adv_data[index++] = 0x02;
  adv_data[index++] = BLUETOOTH_DATA_TYPE_FLAGS;
  adv_data[index++] = 0x06;

  adv_data[index++] = (uint8_t)(name_len + 1);
  adv_data[index++] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
  memcpy(&adv_data[index], DEVICE_NAME, name_len);
  index += name_len;

  adv_data[index++] = 0x03;
  adv_data[index++] =
      BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS;
  adv_data[index++] = ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE & 0xff;
  adv_data[index++] = ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE >> 8;

  adv_data[index++] = 0x03;
  adv_data[index++] = BLUETOOTH_DATA_TYPE_APPEARANCE;
  adv_data[index++] = 0xC0;
  adv_data[index++] = 0x03;

  adv_data_len = (uint8_t)index;
}

#endif // PWMK_ENABLE_BLE
