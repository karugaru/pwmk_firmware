#include "settings/board.h"

// 各行のGPIOピン番号の配列
const uint8_t rows_pins[ROWS] = ROWS_PINS;

// 各列のGPIOピン番号の配列
const uint8_t cols_pins[COLS] = COLS_PINS;

// 各キーの行番号と列番号の配列。-1の場合は未使用。
const uint8_t layout[ROWS * COLS][2] = LAYOUT;
