from __future__ import annotations

from pydantic import BaseModel, Field, model_validator


class CMakeProfile(BaseModel):
    """CMake プロファイルの設定を表す Pydantic モデル。"""

    board: str = Field(min_length=1)
    enable_usb: bool = True
    enable_ble: bool = True


class BoardProfile(BaseModel):
    """ボードプロファイルの設定を表す Pydantic モデル。"""

    rows_pins: list[int] = Field(min_length=1)
    cols_pins: list[int] = Field(min_length=1)
    gpio_sda_pin: int
    gpio_scl_pin: int
    gpio_dr_pin: int
    gpio_led_pin: int
    pin_settle_time_us: int = Field(ge=0)
    layout: list[tuple[int, int]]

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

    user_keycodes: dict[str, str] = Field(default_factory=dict)
    keymap: list[str] = Field(default_factory=list)


class PinnacleProfile(BaseModel):
    """Pinnacle プロファイルの設定を表す Pydantic モデル。"""

    rotate: str = Field(min_length=1)
    accel: float
    speed: float


class SettingsProfile(BaseModel):
    """設定プロファイルの設定を表す Pydantic モデル。"""

    deep_sleep_timeout_seconds: int = Field(ge=0)
    led_brightness: int = Field(ge=1, le=255)
    debounce_time_ms: int = Field(ge=0)
    mouse_move_delta: int = Field(ge=0)
    mouse_move_thresh: int = Field(ge=0)
    mouse_wheel_delta: int = Field(ge=0)
    mouse_wheel_thresh: int = Field(ge=0)
    use_pinnacle: bool = True
    ble_persist_selected_slot: bool = True
    pinnacle: PinnacleProfile | None = None

    @model_validator(mode="after")
    def validate_pinnacle(self) -> SettingsProfile:
        if self.use_pinnacle and self.pinnacle is None:
            raise ValueError(
                "use_pinnacle が指定されていますが、pinnacle の設定がありません。"
            )
        return self


class ProfileConfig(BaseModel):
    """プロファイルの設定を表す Pydantic モデル。"""

    cmake: CMakeProfile
    board: BoardProfile
    keymap: KeymapProfile
    settings: SettingsProfile

    @model_validator(mode="after")
    def validate_keymap(self) -> ProfileConfig:
        expected = self.board.active_layout_count
        if len(self.keymap.keymap) != expected:
            raise ValueError(
                f"キーマップの要素数がアクティブなレイアウトの要素数と一致する必要があります: 期待値 {expected}, 実際の値 {len(self.keymap.keymap)}"
            )
        return self
