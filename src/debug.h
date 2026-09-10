#ifndef PWMK_DEBUG_H
#define PWMK_DEBUG_H

#include <stddef.h>
#include <stdint.h>

void pwmk_debug_printf(const char *module, const char *format, ...);
void pwmk_debug_hexdump(const char *module, const char *label,
                        const uint8_t *data, size_t size);

#endif
