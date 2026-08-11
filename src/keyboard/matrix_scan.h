#ifndef PWMK_MATRIX_SCAN_H
#define PWMK_MATRIX_SCAN_H

#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <stdbool.h>

void matrix_scan_init(void);
void matrix_scan_process(void);
bool matrix_scan_is_pressed(uint8_t row, uint8_t col);

#endif // PWMK_MATRIX_SCAN_H
