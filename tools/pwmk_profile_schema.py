from __future__ import annotations

from pydantic import BaseModel, Field, model_validator


class CMakeProfile(BaseModel):
    """CMake プロファイルの設定を表す Pydantic モデル。"""

    board: str = Field(min_length=1, description="PICO_BOARD に渡すボード名。")
    enable_usb: bool = Field(default=True, description="USB 機能を有効にするかどうか。")
    enable_ble: bool = Field(default=True, description="BLE 機能を有効にするかどうか。")


class BoardProfile(BaseModel):
    """ボードプロファイルの設定を表す Pydantic モデル。"""

    rows_pins: list[int] = Field(
        min_length=1,
        description="キーマトリクスの行ピン番号一覧。",
    )
    cols_pins: list[int] = Field(
        min_length=1,
        description="キーマトリクスの列ピン番号一覧。",
    )
    gpio_sda_pin: int = Field(description="I2C の SDA ピン番号。")
    gpio_scl_pin: int = Field(description="I2C の SCL ピン番号。")
    gpio_dr_pin: int = Field(
        description="Pinnacle のデータレディ信号を受けるピン番号。"
    )
    gpio_led_pin: int = Field(description="ステータス LED を接続する GPIO ピン番号。")
    pin_settle_time_us: int = Field(
        ge=0,
        description="マトリクス走査時にピン状態が安定するまで待つ時間。単位はマイクロ秒。",
    )
    layout: list[tuple[int, int]] = Field(
        description="物理配列とマトリクス座標の対応。未使用位置は [-1, -1] を指定する。"
        " 例として、[[1,1],[3,3]]の場合、keymap[0]が行1列1のスイッチ、keymap[1]が行3列3のスイッチに対応する。",
    )

    @property
    def rows(self) -> int:
        return len(self.rows_pins)

    @property
    def cols(self) -> int:
        return len(self.cols_pins)

    @property
    def key_capacity(self) -> int:
        return self.rows * self.cols

    @property
    def active_layout_count(self) -> int:
        return sum(1 for row, col in self.layout if row >= 0 and col >= 0)

    @model_validator(mode="after")
    def validate_layout(self) -> BoardProfile:
        if len(self.layout) > self.key_capacity:
            raise ValueError("レイアウトの要素数が rows * cols を超えています。")

        seen_positions: set[tuple[int, int]] = set()
        for row, col in self.layout:
            if row == -1 and col == -1:
                continue
            if row < 0 or col < 0:
                raise ValueError(
                    "レイアウトの要素は有効な座標か [-1, -1] である必要があります。"
                )
            if row >= self.rows or col >= self.cols:
                raise ValueError("レイアウトの要素がボードの範囲外です。")
            position = (row, col)
            if position in seen_positions:
                raise ValueError("レイアウトの要素は一意である必要があります。")
            seen_positions.add(position)

        return self


class KeymapProfile(BaseModel):
    """キーマッププロファイルの設定を表す Pydantic モデル。"""

    user_keycodes: dict[str, str] = Field(
        default_factory=dict,
        description="users.c や keymap から参照するユーザー定義キーコード。キーが名前、値が展開先の式。",
    )
    keymap: list[str] = Field(
        default_factory=list,
        description="layout の有効キー数に対応するキーコード一覧。",
    )


class PinnacleProfile(BaseModel):
    """Pinnacle プロファイルの設定を表す Pydantic モデル。"""

    rotate: str = Field(
        min_length=1,
        description="Pinnacle の回転設定。PINNACLE_ROTATE_0 などの定数名を指定する。",
    )
    accel: float = Field(
        description="Pinnacle の加速度設定。移動量は delta^accel*speed で計算される。",
    )
    speed: float = Field(description="Pinnacle の速度設定。")


class SettingsProfile(BaseModel):
    """設定プロファイルの設定を表す Pydantic モデル。"""

    deep_sleep_timeout_seconds: int = Field(
        ge=5,
        description="無操作時にディープスリープへ移行するまでの時間。単位は秒。",
    )
    led_brightness: int = Field(
        ge=1,
        le=255,
        description="LED の明るさ。1 から 255 の範囲で指定する。",
    )
    debounce_time_ms: int = Field(
        ge=0,
        description="キースイッチのデバウンス時間。単位はミリ秒。",
    )
    mouse_move_delta: int = Field(
        ge=0,
        description="マウスキー 1 回分のカーソル移動量。",
    )
    mouse_move_thresh: int = Field(
        ge=0,
        description="マウス移動イベントとして送信するための移動量の閾値。",
    )
    mouse_wheel_delta: int = Field(
        ge=0,
        description="ホイール操作 1 回分の移動量。",
    )
    mouse_wheel_thresh: int = Field(
        ge=0,
        description="ホイール移動イベントとして送信するための移動量の閾値。",
    )
    use_pinnacle: bool = Field(
        default=True,
        description="Pinnacle トラックパッドを使用するかどうか。",
    )
    ble_persist_selected_slot: bool = Field(
        default=True,
        description="選択中の BLE 接続スロットを再起動後も維持するかどうか。",
    )
    pinnacle: PinnacleProfile | None = Field(
        default=None,
        description="Pinnacle 使用時の回転、加速度、速度設定。use_pinnacle が true の場合は必須。",
    )

    @model_validator(mode="after")
    def validate_pinnacle(self) -> SettingsProfile:
        if self.use_pinnacle and self.pinnacle is None:
            raise ValueError(
                "use_pinnacle が指定されていますが、pinnacle の設定がありません。"
            )
        return self


class ProfileConfig(BaseModel):
    """プロファイルの設定を表す Pydantic モデル。"""

    cmake: CMakeProfile = Field(description="ビルド時の CMake 設定。")
    board: BoardProfile = Field(description="基板の配線とレイアウト設定。")
    keymap: KeymapProfile = Field(description="キーコードとキーマップ設定。")
    settings: SettingsProfile = Field(
        description="スリープ、LED、ポインティングデバイスなどの動作設定。"
    )

    @model_validator(mode="after")
    def validate_keymap(self) -> ProfileConfig:
        expected = self.board.active_layout_count
        if len(self.keymap.keymap) != expected:
            raise ValueError(
                f"キーマップの要素数がアクティブなレイアウトの要素数と一致する必要があります: 期待値 {expected}, 実際の値 {len(self.keymap.keymap)}"
            )
        return self
