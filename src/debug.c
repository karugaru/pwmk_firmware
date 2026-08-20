#include "debug.h"

#include <pico/time.h>
#include <stdarg.h>
#include <stdio.h>

/*
 * 内部関数宣言
 */

static void _pwmk_debug_print_prefix(const char *module);

/*
 * 公開関数
 */

void pwmk_debug_printf(const char *module, const char *format, ...) {
  va_list args;
  _pwmk_debug_print_prefix(module);
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

void pwmk_debug_hexdump(const char *module, const char *label,
                        const uint8_t *data, size_t size) {
  _pwmk_debug_print_prefix(module);
  printf("%s:", label);
  for (size_t index = 0; index < size; index++) {
    printf(" 0x%02X", data[index]);
  }
  printf("\n");
}

/*
 * 内部関数
 */

static void _pwmk_debug_print_prefix(const char *module) {
  uint64_t elapsed_ms = to_ms_since_boot(get_absolute_time());
  uint64_t elapsed_hours = elapsed_ms / (60ULL * 60ULL * 1000ULL);
  uint64_t elapsed_minutes = (elapsed_ms / (60ULL * 1000ULL)) % 60ULL;
  uint64_t elapsed_seconds = (elapsed_ms / 1000ULL) % 60ULL;
  uint64_t milliseconds = elapsed_ms % 1000ULL;

  printf("[%02llu:%02llu:%02llu.%03llu] %s ", (unsigned long long)elapsed_hours,
         (unsigned long long)elapsed_minutes,
         (unsigned long long)elapsed_seconds, (unsigned long long)milliseconds,
         module);
}
