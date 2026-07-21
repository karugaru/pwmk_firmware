#ifndef PWMK_BLE_H
#define PWMK_BLE_H

#include "../hid/hid.h"
#include <stdbool.h>
#include <stdint.h>

void ble_setup(void);
void ble_power_set(bool power);
void ble_poll(void);
bool ble_is_connected(void);
bool ble_is_enabled(void);
void ble_request_can_send(void);
void ble_reconnect(void);
bool ble_select_slot(uint8_t slot);
bool ble_unpair_selected_slot(void);

#endif // PWMK_BLE_H
