#ifndef PWMK_ADVERTISING_DATA_H
#define PWMK_ADVERTISING_DATA_H

#include <pico/stdlib.h>

extern uint8_t adv_data[];
extern uint8_t adv_data_len;

void advertising_data_init(void);

#endif // PWMK_ADVERTISING_DATA_H
