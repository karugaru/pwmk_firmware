#include "ble/ble.h"

#ifndef uint8_t
#define uint8_t unsigned char
#endif

/*
 * 公開関数
 */

__attribute__((weak)) void ble_setup(void) {}

__attribute__((weak)) void ble_power_set(bool power) { (void)power; }

__attribute__((weak)) void ble_poll(void) {}

__attribute__((weak)) bool ble_is_connected(void) { return false; }

__attribute__((weak)) bool ble_is_enabled(void) { return false; }

__attribute__((weak)) void ble_request_can_send(void) {}

__attribute__((weak)) void ble_reconnect(void) {}

__attribute__((weak)) bool ble_select_slot(uint8_t slot) {
  (void)slot;
  return false;
}

__attribute__((weak)) bool ble_unpair_selected_slot(void) { return false; }
