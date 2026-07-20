#include "le_device_db_tlv_custom.h"
#include "ble/core.h"
#include "ble/le_device_db.h"
#include "ble/le_device_db_tlv.h"
#include "btstack_debug.h"
#include "gap.h"
#include "le_device_db_tlv_custom.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef BD_ADDR_COPY
#define BD_ADDR_COPY(dest, src) memcpy((dest), (src), sizeof(bd_addr_t))
#endif

#define LE_DEVICE_DB_SLOT_COUNT 4

/**
 * @brief 保留中のスロット操作種別。
 */
typedef enum {
  /** 操作なし。 */
  LE_DEVICE_DB_PENDING_NONE = 0,
  /** 別スロットへの切替を保留中。 */
  LE_DEVICE_DB_PENDING_SELECT,
  /** 選択中スロットの削除を保留中。 */
  LE_DEVICE_DB_PENDING_CLEAR,
} le_device_db_pending_action_t;

/**
 * @brief 1スロット分の永続化ボンド情報。
 */
typedef struct {
  /** 登録順を表すシーケンス番号。 */
  uint32_t seq_nr;
  /** 相手機器のBluetoothアドレス種別。 */
  int32_t addr_type;
  /** 相手機器のBluetoothアドレス。 */
  bd_addr_t addr;
  /** 相手機器のIRK。 */
  sm_key_t irk;
  /** 再暗号化に使うLTK。 */
  sm_key_t ltk;
  /** LTKに対応するEDIV。 */
  uint16_t ediv;
  /** LTKに対応するRAND。 */
  uint8_t rand[8];
  /** 鍵長。 */
  uint8_t key_size;
  /** 認証済みかどうか。 */
  uint8_t authenticated;
  /** 認可済みかどうか。 */
  uint8_t authorized;
  /** Secure Connectionsで確立した鍵かどうか。 */
  uint8_t secure_connection;
} le_device_db_persisted_entry_t;

/**
 * @brief 1スロット分のキャッシュ状態。
 */
typedef struct {
  /** このスロットに有効なボンド情報があるかどうか。 */
  uint8_t used;
  /** スロットに紐づく永続化ボンド情報のキャッシュ。 */
  le_device_db_persisted_entry_t data;
} le_device_db_slot_t;

/**
 * @brief LE Device DB 実装全体のランタイム状態。
 */
typedef struct {
  /** 使用中のTLVバックエンド実装。 */
  const btstack_tlv_t *tlv_impl;
  /** TLVバックエンドへ渡すコンテキスト。 */
  void *tlv_context;
  /** 現在pwmkが選択しているスロット番号。 */
  int selected_slot;
  /** 次回適用予定のスロット番号。 */
  int pending_slot;
  /** 次回適用予定のスロット操作種別。 */
  le_device_db_pending_action_t pending_action;
  /** 各スロットのAdvertisingアドレス世代番号。 */
  uint8_t address_generation[LE_DEVICE_DB_SLOT_COUNT];
  /** 各スロットのボンド情報キャッシュ。 */
  le_device_db_slot_t slots[LE_DEVICE_DB_SLOT_COUNT];
} le_device_db_state_t;

static le_device_db_state_t le_device_db_state = {
    0,
    0,
    0,
    0,
    LE_DEVICE_DB_PENDING_NONE,
    {0, 0, 0, 0},
    {{0, {0}}, {0, {0}}, {0, {0}}, {0, {0}}}};

/**
 * @brief TLV保存用の32bitタグを生成する。
 * @param a タグ1文字目。
 * @param b タグ2文字目。
 * @param c タグ3文字目。
 * @param index タグ末尾に入れるインデックス値。
 * @return 生成したタグ値。
 */
static uint32_t le_device_db_tag(char a, char b, char c, uint8_t index) {
  return ((uint32_t)(uint8_t)a << 24) | ((uint32_t)(uint8_t)b << 16) |
         ((uint32_t)(uint8_t)c << 8) | index;
}

/**
 * @brief ボンド情報を保存するTLVタグをスロット番号から生成する。
 * @param slot 対象スロット番号。
 * @return ボンド情報用のTLVタグ。
 */
static uint32_t le_device_db_entry_tag(int slot) {
  return le_device_db_tag('B', 'S', 'D', (uint8_t)slot);
}

/**
 * @brief 現在選択中スロットを保存するTLVタグを返す。
 * @return selected_slot 保存用のTLVタグ。
 */
static uint32_t le_device_db_selected_slot_tag(void) {
  return le_device_db_tag('B', 'S', 'L', 0);
}

/**
 * @brief スロットごとのAdvertisingアドレス世代を保存するTLVタグを返す。
 * @return address_generation 保存用のTLVタグ。
 */
static uint32_t le_device_db_generation_tag(void) {
  return le_device_db_tag('B', 'S', 'G', 0);
}

/**
 * @brief スロット番号が管理範囲内かを判定する。
 * @param index 検査対象スロット番号。
 * @return 有効な場合は true。
 */
static bool le_device_db_slot_valid(int index) {
  return (index >= 0) && (index < LE_DEVICE_DB_SLOT_COUNT);
}

/**
 * @brief TLVバックエンドが利用可能な状態かを判定する。
 * @return 利用可能な場合は true。
 */
static bool le_device_db_tlv_ready(void) {
  return (le_device_db_state.tlv_impl != 0) &&
         (le_device_db_state.tlv_impl->get_tag != 0) &&
         (le_device_db_state.tlv_impl->store_tag != 0) &&
         (le_device_db_state.tlv_impl->delete_tag != 0);
}

/**
 * @brief 永続化エントリを未使用状態として初期化する。
 * @param entry 初期化対象エントリ。
 */
static void le_device_db_zero_entry(le_device_db_persisted_entry_t *entry) {
  // 未使用エントリと判別できるよう、全体をゼロ化した後に addr_type を unknown
  // にする。
  memset(entry, 0, sizeof(*entry));
  entry->addr_type = BD_ADDR_TYPE_UNKNOWN;
}

/**
 * @brief 指定スロットのキャッシュ状態を未使用に戻す。
 * @param slot 対象スロット番号。
 */
static void le_device_db_clear_slot_state(int slot) {
  if (!le_device_db_slot_valid(slot)) {
    return;
  }
  // 永続領域ではなくメモリ上のキャッシュだけを消す。
  le_device_db_state.slots[slot].used = 0;
  le_device_db_zero_entry(&le_device_db_state.slots[slot].data);
}

/**
 * @brief 指定スロットのボンド情報をTLVから読み出す。
 * @param slot 対象スロット番号。
 * @param entry 読み出し先。
 * @return 読み出し成功時は true。
 */
static bool le_device_db_read_entry(int slot,
                                    le_device_db_persisted_entry_t *entry) {
  int size;

  if (!le_device_db_slot_valid(slot) || (entry == 0) ||
      !le_device_db_tlv_ready()) {
    return false;
  }

  // 読み出し失敗時にゴミが残らないよう、先に既定値で初期化しておく。
  le_device_db_zero_entry(entry);
  size = le_device_db_state.tlv_impl->get_tag(le_device_db_state.tlv_context,
                                              le_device_db_entry_tag(slot),
                                              (uint8_t *)entry, sizeof(*entry));
  if (size != (int)sizeof(*entry)) {
    return false;
  }
  return true;
}

/**
 * @brief 指定スロットのボンド情報をTLVへ保存し、キャッシュも更新する。
 * @param slot 対象スロット番号。
 * @param entry 保存するボンド情報。
 * @return 保存成功時は true。
 */
static bool
le_device_db_write_entry(int slot,
                         const le_device_db_persisted_entry_t *entry) {
  int status;

  if (!le_device_db_slot_valid(slot) || (entry == 0) ||
      !le_device_db_tlv_ready()) {
    return false;
  }

  status = le_device_db_state.tlv_impl->store_tag(
      le_device_db_state.tlv_context, le_device_db_entry_tag(slot),
      (const uint8_t *)entry, sizeof(*entry));
  if (status != 0) {
    return false;
  }

  // 永続化成功後にのみメモリ上のキャッシュを更新する。
  le_device_db_state.slots[slot].used = 1;
  le_device_db_state.slots[slot].data = *entry;
  return true;
}

/**
 * @brief 指定スロットのボンド情報を削除する。
 * @param slot 対象スロット番号。
 */
static void le_device_db_delete_entry(int slot) {
  if (!le_device_db_slot_valid(slot)) {
    return;
  }

  if (le_device_db_tlv_ready()) {
    // 永続領域から削除できる場合は先にTLVを削除する。
    le_device_db_state.tlv_impl->delete_tag(le_device_db_state.tlv_context,
                                            le_device_db_entry_tag(slot));
  }
  // TLV削除可否にかかわらず、キャッシュは未使用状態へ揃える。
  le_device_db_clear_slot_state(slot);
}

/**
 * @brief 現在の selected_slot をTLVへ保存する。
 * @return 保存成功時は true。
 */
static bool le_device_db_save_selected_slot(void) {
  uint8_t value;

  if (!le_device_db_tlv_ready()) {
    return false;
  }

  value = (uint8_t)le_device_db_state.selected_slot;
  return le_device_db_state.tlv_impl->store_tag(
             le_device_db_state.tlv_context, le_device_db_selected_slot_tag(),
             &value, sizeof(value)) == 0;
}

/**
 * @brief 現在選択中スロットをTLVから読み込む。
 */
static void le_device_db_load_selected_slot(void) {
  uint8_t value = 0;
  int size;

  // TLVに値がない場合も slot 0 を既定値として扱う。
  le_device_db_state.selected_slot = 0;
  if (!le_device_db_tlv_ready()) {
    return;
  }

  size = le_device_db_state.tlv_impl->get_tag(le_device_db_state.tlv_context,
                                              le_device_db_selected_slot_tag(),
                                              &value, sizeof(value));
  if ((size == (int)sizeof(value)) && le_device_db_slot_valid((int)value)) {
    le_device_db_state.selected_slot = (int)value;
    return;
  }

  le_device_db_state.selected_slot = 0;
  // 無効値しかない場合は既定値を書き戻し、以後の動作を安定させる。
  (void)le_device_db_save_selected_slot();
}

/**
 * @brief スロットごとのAdvertisingアドレス世代をTLVへ保存する。
 * @return 保存成功時は true。
 */
static bool le_device_db_save_address_generations(void) {
  if (!le_device_db_tlv_ready()) {
    return false;
  }

  return le_device_db_state.tlv_impl->store_tag(
             le_device_db_state.tlv_context, le_device_db_generation_tag(),
             le_device_db_state.address_generation,
             sizeof(le_device_db_state.address_generation)) == 0;
}

/**
 * @brief スロットごとのAdvertisingアドレス世代をTLVから読み込む。
 */
static void le_device_db_load_address_generations(void) {
  int size;

  // 読み出し失敗時に古い値を残さないよう、先にゼロ初期化する。
  memset(le_device_db_state.address_generation, 0,
         sizeof(le_device_db_state.address_generation));
  if (!le_device_db_tlv_ready()) {
    return;
  }

  size = le_device_db_state.tlv_impl->get_tag(
      le_device_db_state.tlv_context, le_device_db_generation_tag(),
      le_device_db_state.address_generation,
      sizeof(le_device_db_state.address_generation));
  if (size == (int)sizeof(le_device_db_state.address_generation)) {
    return;
  }

  memset(le_device_db_state.address_generation, 0,
         sizeof(le_device_db_state.address_generation));
  // 未保存時は全スロット世代 0 を永続化して扱いを明示する。
  (void)le_device_db_save_address_generations();
}

/**
 * @brief 全スロットのボンド情報をTLVから読み込み、キャッシュを再構築する。
 */
static void le_device_db_load_slots(void) {
  int slot;

  for (slot = 0; slot < LE_DEVICE_DB_SLOT_COUNT; slot++) {
    // 毎回キャッシュを消してからTLVの実内容で復元する。
    le_device_db_clear_slot_state(slot);
    if (le_device_db_read_entry(slot, &le_device_db_state.slots[slot].data)) {
      le_device_db_state.slots[slot].used = 1;
    }
  }
}

/**
 * @brief 次に採番するシーケンス番号を求める。
 * @return 次に使う seq_nr。
 */
static uint32_t le_device_db_next_seq_nr(void) {
  int slot;
  uint32_t max_seq = 0;

  for (slot = 0; slot < LE_DEVICE_DB_SLOT_COUNT; slot++) {
    if (!le_device_db_state.slots[slot].used) {
      continue;
    }
    if (le_device_db_state.slots[slot].data.seq_nr > max_seq) {
      max_seq = le_device_db_state.slots[slot].data.seq_nr;
    }
  }
  return max_seq + 1u;
}

/**
 * @brief BTstackから見えるインデックスかを判定する。
 * @param index 判定対象インデックス。
 * @return 選択中スロットに一致する場合は true。
 */
static bool le_device_db_visible_index(int index) {
  return le_device_db_slot_valid(index) &&
         (index == le_device_db_state.selected_slot);
}

/**
 * @brief 現在選択中スロットのボンド情報を即時削除し、世代番号を進める。
 * @return 実行できた場合は true。
 */
static bool le_device_db_clear_selected_slot_now(void) {
  int slot = le_device_db_state.selected_slot;
  le_device_db_persisted_entry_t *entry;

  if (!le_device_db_slot_valid(slot)) {
    return false;
  }

  if (le_device_db_state.slots[slot].used) {
    entry = &le_device_db_state.slots[slot].data;
    if (entry->addr_type != BD_ADDR_TYPE_UNKNOWN) {
      // 既知の接続先情報がある場合は、BTstack側のbonding情報も消す。
      gap_delete_bonding((bd_addr_type_t)entry->addr_type, entry->addr);
    } else {
      // アドレス種別不明のエントリはローカル保存だけ削除する。
      le_device_db_delete_entry(slot);
    }

    if (entry->addr_type != BD_ADDR_TYPE_UNKNOWN) {
      // bonding削除後に、この実装のTLVエントリも必ず消す。
      le_device_db_delete_entry(slot);
    }
  }

  // 次回Advertisingで旧ホストに同一個体と見なされないよう世代を進める。
  le_device_db_state.address_generation[slot]++;
  (void)le_device_db_save_address_generations();
  return true;
}

/**
 * @brief TLVバックエンドを登録し、永続状態を読み込む。
 * @param btstack_tlv_impl 使用するTLV実装。
 * @param btstack_tlv_context TLV実装へ渡すコンテキスト。
 * @details 想定呼び出し元は BTstack。
 */
void le_device_db_tlv_configure(const btstack_tlv_t *btstack_tlv_impl,
                                void *btstack_tlv_context) {
  le_device_db_state.tlv_impl = btstack_tlv_impl;
  le_device_db_state.tlv_context = btstack_tlv_context;
  // TLVバックエンド確定後に永続状態を読み込み、以後の問い合わせに備える。
  le_device_db_init();
}

/**
 * @brief スロット切替を保留状態として予約する。
 * @param index 切替先スロット番号。
 * @return 予約に成功した場合は true。
 * @details 想定呼び出し元は pwmk。
 */
bool le_device_db_tlv_schedule_select_slot(int index) {
  if (!le_device_db_slot_valid(index)) {
    return false;
  }

  // 実際の切替は再接続タイミングで apply 側が行う。
  le_device_db_state.pending_action = LE_DEVICE_DB_PENDING_SELECT;
  le_device_db_state.pending_slot = index;
  return true;
}

/**
 * @brief 現在選択中スロットの削除を保留状態として予約する。
 * @return 予約に成功した場合は true。
 * @details 想定呼び出し元は pwmk。
 */
bool le_device_db_tlv_schedule_clear_selected_slot(void) {
  if (!le_device_db_slot_valid(le_device_db_state.selected_slot)) {
    return false;
  }

  // 実際の削除は切断や再起動後の安全なタイミングで apply 側が行う。
  le_device_db_state.pending_action = LE_DEVICE_DB_PENDING_CLEAR;
  le_device_db_state.pending_slot = le_device_db_state.selected_slot;
  return true;
}

/**
 * @brief 保留中のスロット操作を適用する。
 * @return 状態が変化した場合は true。
 * @details 想定呼び出し元は pwmk。
 */
bool le_device_db_tlv_apply_pending_slot_action(void) {
  bool changed = false;

  switch (le_device_db_state.pending_action) {
  case LE_DEVICE_DB_PENDING_NONE:
    return false;
  case LE_DEVICE_DB_PENDING_SELECT:
    if (!le_device_db_slot_valid(le_device_db_state.pending_slot)) {
      return false;
    }
    // 切替先を確定し、永続領域にも反映する。
    le_device_db_state.selected_slot = le_device_db_state.pending_slot;
    (void)le_device_db_save_selected_slot();
    changed = true;
    break;
  case LE_DEVICE_DB_PENDING_CLEAR:
    if (!le_device_db_slot_valid(le_device_db_state.selected_slot)) {
      return false;
    }
    changed = le_device_db_clear_selected_slot_now();
    break;
  default:
    return false;
  }

  if (changed) {
    // 反映が終わったら pending 状態を消し、現在スロットに同期する。
    le_device_db_state.pending_action = LE_DEVICE_DB_PENDING_NONE;
    le_device_db_state.pending_slot = le_device_db_state.selected_slot;
  }
  return changed;
}

/**
 * @brief 現在選択中のスロット番号を返す。
 * @return 選択中スロット番号。
 * @details 想定呼び出し元は pwmk。
 */
int le_device_db_tlv_get_selected_slot(void) {
  return le_device_db_state.selected_slot;
}

/**
 * @brief 指定スロットのAdvertisingアドレス世代を返す。
 * @param index 対象スロット番号。
 * @return 世代番号。無効スロット時は 0。
 * @details 想定呼び出し元は pwmk。
 */
uint8_t le_device_db_tlv_get_slot_address_generation(int index) {
  if (!le_device_db_slot_valid(index)) {
    return 0;
  }
  return le_device_db_state.address_generation[index];
}

/**
 * @brief LE Device DB のランタイム状態を初期化し、必要ならTLVから復元する。
 * @details 想定呼び出し元は BTstack。
 */
void le_device_db_init(void) {
  int slot;

  // pending 操作は起動時に持ち越さず、毎回クリアして開始する。
  le_device_db_state.pending_action = LE_DEVICE_DB_PENDING_NONE;
  le_device_db_state.pending_slot = le_device_db_state.selected_slot;
  for (slot = 0; slot < LE_DEVICE_DB_SLOT_COUNT; slot++) {
    le_device_db_clear_slot_state(slot);
  }

  if (!le_device_db_tlv_ready()) {
    // TLV未設定時は揮発状態だけ既定値に戻して終了する。
    le_device_db_state.selected_slot = 0;
    memset(le_device_db_state.address_generation, 0,
           sizeof(le_device_db_state.address_generation));
    return;
  }

  // 永続化済みの selected slot / generation / bond 情報を順に復元する。
  le_device_db_load_selected_slot();
  le_device_db_load_address_generations();
  le_device_db_load_slots();
  le_device_db_state.pending_slot = le_device_db_state.selected_slot;
}

/**
 * @brief ローカルBDアドレス設定要求を受けるが、この実装では保持しない。
 * @param bd_addr 設定要求されたアドレス。
 * @details 想定呼び出し元は BTstack。
 */
void le_device_db_set_local_bd_addr(bd_addr_t bd_addr) { UNUSED(bd_addr); }

/**
 * @brief 現在選択中スロットに見えているボンド件数を返す。
 * @return 0 または 1。
 * @details 想定呼び出し元は BTstack。
 */
int le_device_db_count(void) {
  int slot = le_device_db_state.selected_slot;

  if (!le_device_db_slot_valid(slot)) {
    return 0;
  }
  return le_device_db_state.slots[slot].used ? 1 : 0;
}

/**
 * @brief この実装が内部管理しているスロット総数を返す。
 * @return スロット総数。
 * @details 想定呼び出し元は BTstack。
 */
int le_device_db_max_count(void) { return LE_DEVICE_DB_SLOT_COUNT; }

/**
 * @brief 指定インデックスのボンド情報を削除する。
 * @param index 削除対象インデックス。
 * @details 想定呼び出し元は BTstack。
 */
void le_device_db_remove(int index) {
  if (!le_device_db_visible_index(index)) {
    return;
  }
  if (!le_device_db_state.slots[index].used) {
    return;
  }
  // BTstackから見えるのは選択中スロットのみなので、その1件だけ削除する。
  le_device_db_delete_entry(index);
}

/**
 * @brief 現在選択中スロットへ新しいボンド情報を登録する。
 * @param addr_type 相手機器アドレス種別。
 * @param addr 相手機器アドレス。
 * @param irk 相手機器のIRK。
 * @return 登録したインデックス。失敗時は -1。
 * @details 想定呼び出し元は BTstack。
 */
int le_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk) {
  int slot = le_device_db_state.selected_slot;
  le_device_db_persisted_entry_t entry;

  if (!le_device_db_slot_valid(slot) || !le_device_db_tlv_ready()) {
    return -1;
  }

  if (le_device_db_state.slots[slot].used) {
    // 同じスロットを上書き再利用し、既存の暗号化情報も必要なら引き継ぐ。
    entry = le_device_db_state.slots[slot].data;
  } else {
    le_device_db_zero_entry(&entry);
  }

  // スロット単位で最新のボンド情報に置き換える。
  entry.seq_nr = le_device_db_next_seq_nr();
  entry.addr_type = addr_type;
  BD_ADDR_COPY(entry.addr, addr);
  memcpy(entry.irk, irk, sizeof(entry.irk));

  if (!le_device_db_write_entry(slot, &entry)) {
    return -1;
  }
  return slot;
}

/**
 * @brief 指定インデックスの基本ボンド情報を返す。
 * @param index 取得対象インデックス。
 * @param addr_type 相手機器アドレス種別の出力先。
 * @param addr 相手機器アドレスの出力先。
 * @param irk 相手機器IRKの出力先。
 * @details 想定呼び出し元は BTstack。
 */
void le_device_db_info(int index, int *addr_type, bd_addr_t addr,
                       sm_key_t irk) {
  // 未使用スロット時でも呼び出し側が安全に扱えるよう、先に既定値で埋める。
  if (addr_type != 0) {
    *addr_type = BD_ADDR_TYPE_UNKNOWN;
  }
  if (addr != 0) {
    memset(addr, 0, sizeof(bd_addr_t));
  }
  if (irk != 0) {
    memset(irk, 0, sizeof(sm_key_t));
  }

  if (!le_device_db_visible_index(index) ||
      !le_device_db_state.slots[index].used) {
    return;
  }

  // 選択中スロットに保存されている1件だけを返す。
  if (addr_type != 0) {
    *addr_type = le_device_db_state.slots[index].data.addr_type;
  }
  if (addr != 0) {
    BD_ADDR_COPY(addr, le_device_db_state.slots[index].data.addr);
  }
  if (irk != 0) {
    memcpy(irk, le_device_db_state.slots[index].data.irk, sizeof(sm_key_t));
  }
}

/**
 * @brief 指定インデックスの暗号化情報を更新する。
 * @param index 更新対象インデックス。
 * @param ediv EDIV。
 * @param rand RAND。
 * @param ltk LTK。
 * @param key_size 鍵長。
 * @param authenticated 認証済みフラグ。
 * @param authorized 認可済みフラグ。
 * @param secure_connection Secure Connections フラグ。
 * @details 想定呼び出し元は BTstack。
 */
void le_device_db_encryption_set(int index, uint16_t ediv, uint8_t rand[8],
                                 sm_key_t ltk, int key_size, int authenticated,
                                 int authorized, int secure_connection) {
  le_device_db_persisted_entry_t entry;

  if (!le_device_db_visible_index(index) ||
      !le_device_db_state.slots[index].used) {
    return;
  }

  // 既存エントリを複製し、暗号化関連フィールドだけを更新する。
  entry = le_device_db_state.slots[index].data;
  entry.ediv = ediv;
  memcpy(entry.rand, rand, sizeof(entry.rand));
  memcpy(entry.ltk, ltk, sizeof(entry.ltk));
  entry.key_size = (uint8_t)key_size;
  entry.authenticated = (uint8_t)authenticated;
  entry.authorized = (uint8_t)authorized;
  entry.secure_connection = (uint8_t)secure_connection;
  (void)le_device_db_write_entry(index, &entry);
}

/**
 * @brief 指定インデックスの暗号化情報を取得する。
 * @param index 取得対象インデックス。
 * @param ediv EDIVの出力先。
 * @param rand RANDの出力先。
 * @param ltk LTKの出力先。
 * @param key_size 鍵長の出力先。
 * @param authenticated 認証済みフラグの出力先。
 * @param authorized 認可済みフラグの出力先。
 * @param secure_connection Secure Connections フラグの出力先。
 * @details 想定呼び出し元は BTstack。
 */
void le_device_db_encryption_get(int index, uint16_t *ediv, uint8_t rand[8],
                                 sm_key_t ltk, int *key_size,
                                 int *authenticated, int *authorized,
                                 int *secure_connection) {
  // データ未登録時でも呼び出し側が安全に扱えるよう、先にゼロクリアしておく。
  if (ediv != 0) {
    *ediv = 0;
  }
  if (rand != 0) {
    memset(rand, 0, 8u);
  }
  if (ltk != 0) {
    memset(ltk, 0, sizeof(sm_key_t));
  }
  if (key_size != 0) {
    *key_size = 0;
  }
  if (authenticated != 0) {
    *authenticated = 0;
  }
  if (authorized != 0) {
    *authorized = 0;
  }
  if (secure_connection != 0) {
    *secure_connection = 0;
  }

  if (!le_device_db_visible_index(index) ||
      !le_device_db_state.slots[index].used) {
    return;
  }

  // 選択中スロットに紐づく暗号化情報のみを返す。
  if (ediv != 0) {
    *ediv = le_device_db_state.slots[index].data.ediv;
  }
  if (rand != 0) {
    memcpy(rand, le_device_db_state.slots[index].data.rand,
           sizeof(le_device_db_state.slots[index].data.rand));
  }
  if (ltk != 0) {
    memcpy(ltk, le_device_db_state.slots[index].data.ltk, sizeof(sm_key_t));
  }
  if (key_size != 0) {
    *key_size = le_device_db_state.slots[index].data.key_size;
  }
  if (authenticated != 0) {
    *authenticated = le_device_db_state.slots[index].data.authenticated;
  }
  if (authorized != 0) {
    *authorized = le_device_db_state.slots[index].data.authorized;
  }
  if (secure_connection != 0) {
    *secure_connection = le_device_db_state.slots[index].data.secure_connection;
  }
}

/**
 * @brief 登録済みスロットをダンプするための拡張ポイント。
 * @details 想定呼び出し元は BTstack または pwmk のデバッグ用途。
 */
void le_device_db_dump(void) {
  int slot;

  for (slot = 0; slot < LE_DEVICE_DB_SLOT_COUNT; slot++) {
    // 現状は出力先を持たないため、使用中スロットの走査だけ残している。
    if (!le_device_db_state.slots[slot].used) {
      continue;
    }
  }
}
