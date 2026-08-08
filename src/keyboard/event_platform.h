#ifndef PWMK_EVENT_PLATFORM_H
#define PWMK_EVENT_PLATFORM_H

#include "keyboard/code.h"
#include <stdbool.h>

bool event_process_platform(icode_t icode, bool pressed);

#endif // PWMK_EVENT_PLATFORM_H
