#include "settings/settings.h"
#include "settings/keymap.h"
#include "settings/persistence.h"

/**
 * @brief
 * プロファイル設定に必要な初期化を行う。
 */
void settings_init(void) {
  keymap_init();
  persistence_init();
}
