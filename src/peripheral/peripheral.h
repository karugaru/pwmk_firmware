#ifndef PWMK_PERIPHERAL_H
#define PWMK_PERIPHERAL_H

#include "state/state.h"
#include <pico/stdlib.h>
#include <stdio.h>

bool peripheral_init(void);
bool peripheral_require_event_processing(void);
void peripheral_process_events(void);
void peripheral_process_periodic(void);
void peripheral_early_init(void);

#endif // PWMK_PERIPHERAL_H
