#include "settings/board.h"

// 各行のGPIOピン番号の配列
const uint8_t rows_pins[ROWS] = ROWS_PINS;

// 各列のGPIOピン番号の配列
const uint8_t cols_pins[COLS] = COLS_PINS;

// 各キーの行番号と列番号の配列。
// 行番号と列番号は回路的な配置を示すもので、キーの物理的な配置とは異なる場合がある。
// 行番号RのGPIOピンはrows_pins[R]、列番号CのGPIOピンはcols_pins[C]となる。
const int8_t layout[ROWS * COLS][2] = LAYOUT;
