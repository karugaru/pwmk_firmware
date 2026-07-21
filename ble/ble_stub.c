#include "ble.h"

#ifndef uint8_t
#define uint8_t unsigned char
#endif

void ble_setup(void) {}

void ble_power_set(bool power) { (void)power; }

void ble_poll(void) {}

bool ble_is_connected(void) { return false; }

bool ble_is_enabled(void) { return false; }

void ble_request_can_send(void) {}

void ble_reconnect(void) {}

bool ble_select_slot(uint8_t slot) {
  (void)slot;
  return false;
}

bool ble_unpair_selected_slot(void) { return false; }
