#ifndef LE_DEVICE_DB_TLV_CUSTOM_H
#define LE_DEVICE_DB_TLV_CUSTOM_H

#include <btstack.h>
#include <stdbool.h>
#include <stdint.h>

bool le_device_db_tlv_schedule_select_slot(int index);
bool le_device_db_tlv_schedule_clear_selected_slot(void);
bool le_device_db_tlv_apply_pending_slot_action(void);
int le_device_db_tlv_get_selected_slot(void);
uint8_t le_device_db_tlv_get_slot_address_generation(int index);

#endif // LE_DEVICE_DB_TLV_CUSTOM_H
