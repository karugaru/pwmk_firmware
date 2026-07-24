from __future__ import annotations

import shutil
from pathlib import Path
from typing import Annotated, Any

import typer
import yaml
from jinja2 import Environment, FileSystemLoader, StrictUndefined
from pwmk_common import ensure_directory, repo_root
from pydantic import BaseModel, Field, ValidationError, model_validator

USERS_DIR_NAME = "users"
CURRENT_PROFILE_FILE_NAME = ".current_profile"


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


def users_root() -> Path:
    """ユーザープロファイルのルートディレクトリを返す。"""
    return repo_root() / USERS_DIR_NAME


def current_profile_path() -> Path:
    """現在のプロファイルのパスを返す。"""
    return users_root() / CURRENT_PROFILE_FILE_NAME


def profile_dir(profile_name: str) -> Path:
    """指定されたユーザープロファイルのディレクトリを返す。"""
    return users_root() / profile_name


def profile_yaml_path(profile_name: str) -> Path:
    """指定されたユーザープロファイルの YAML ファイルのパスを返す。"""
    return profile_dir(profile_name) / "profile.yaml"


def profile_users_c_path(profile_name: str) -> Path:
    """指定されたユーザープロファイルの users.c ファイルのパスを返す。"""
    return profile_dir(profile_name) / "users.c"


def template_root() -> Path:
    """プロファイルテンプレートのルートディレクトリを返す。"""
    return repo_root() / "tools" / "templates" / "profile"


def jinja_environment() -> Environment:
    """Jinja2 のテンプレート環境を返す。"""
    environment = Environment(
        loader=FileSystemLoader(str(template_root())),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )
    environment.filters["c_bool"] = lambda value: 1 if value else 0
    environment.filters["cmake_bool"] = lambda value: "ON" if value else "OFF"
    environment.filters["c_float"] = c_float_literal
    return environment


def c_float_literal(value: float) -> str:
    """浮動小数点数を C の浮動小数点リテラルとして返す。"""
    text = f"{value:g}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return f"{text}f"


def render_template(template_name: str, context: dict[str, Any]) -> str:
    """
    指定されたテンプレートをレンダリングして文字列として返す。

    :param template_name: テンプレートファイル名。
    :param context: テンプレートに渡すコンテキスト。
    :return: レンダリングされた文字列。
    """
    return jinja_environment().get_template(template_name).render(**context)


def ensure_profile_exists(profile_name: str) -> None:
    """指定されたプロファイルが存在することを確認する。存在しない場合は SystemExit を発生させる。"""
    yaml_path = profile_yaml_path(profile_name)
    users_c_path = profile_users_c_path(profile_name)

    missing_paths = [path for path in (yaml_path, users_c_path) if not path.exists()]
    if missing_paths:
        missing = ", ".join(
            str(path.relative_to(repo_root())) for path in missing_paths
        )
        raise SystemExit(f"プロファイルが見つかりません: {missing}")


def active_profile_name() -> str | None:
    """現在のアクティブなプロファイル名を返す。選択されていない場合は None を返す。"""
    path = current_profile_path()
    if not path.exists():
        return None

    value = path.read_text(encoding="utf-8").strip()
    return value or None


def require_active_profile_name() -> str:
    """現在のアクティブなプロファイル名を返す。選択されていない場合は SystemExit を発生させる。"""
    profile_name = active_profile_name()
    if profile_name is None:
        raise SystemExit(
            "プロファイルが選択されていません。`uv run tools/pwmk.py profile <profile>` を実行してプロファイルを選択してください。"
        )
    ensure_profile_exists(profile_name)
    return profile_name


def clean_build_directory() -> None:
    """ビルドディレクトリを削除する。"""
    build_root = repo_root() / "build"
    if build_root.exists():
        shutil.rmtree(build_root)


def select_profile(profile_name: str) -> None:
    """指定されたプロファイルをアクティブにする。"""
    ensure_profile_exists(profile_name)
    ensure_directory(users_root())
    current_profile_path().write_text(f"{profile_name}\n", encoding="utf-8")
    clean_build_directory()


def clear_profile_selection() -> None:
    """現在のプロファイルの選択を解除する。"""
    path = current_profile_path()
    if path.exists():
        path.unlink()
    clean_build_directory()


def load_profile_config(profile_name: str) -> ProfileConfig:
    """指定されたプロファイルの設定を読み込み、ProfileConfig オブジェクトとして返す。"""
    ensure_profile_exists(profile_name)

    data = yaml.safe_load(profile_yaml_path(profile_name).read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise SystemExit(
            "profile.yaml のトップレベルはマッピングである必要があります。"
        )

    try:
        return ProfileConfig.model_validate(data)
    except ValidationError as error:
        raise SystemExit(str(error)) from error


def padded_layout(
    layout: list[tuple[int, int]], capacity: int
) -> list[tuple[int, int]]:
    """
    レイアウトを指定された容量に合わせてパディングする。

    :param layout: レイアウトのリスト。
    :param capacity: パディング後の容量。
    :return: パディングされたレイアウトのリスト。
    """

    if len(layout) >= capacity:
        return list(layout)
    return list(layout) + [(-1, -1)] * (capacity - len(layout))


def generated_profile_root(build_dir: Path) -> Path:
    """ビルドディレクトリ内の生成されたプロファイルのルートディレクトリを返す。"""
    return build_dir / "generated" / "profile"


def generated_src_dir(build_dir: Path) -> Path:
    """ビルドディレクトリ内の生成されたソースディレクトリを返す。"""
    return generated_profile_root(build_dir) / "src"


def generated_settings_dir(build_dir: Path) -> Path:
    """ビルドディレクトリ内の生成された設定ディレクトリを返す。"""
    return generated_src_dir(build_dir) / "settings"


def generated_profile_cmake_path(build_dir: Path) -> Path:
    """ビルドディレクトリ内の生成された profile.cmake のパスを返す。"""
    return generated_profile_root(build_dir) / "profile.cmake"


def write_generated_file(path: Path, content: str) -> None:
    """
    文字列をファイルに書き込む。

    :param path: 書き込むファイルのパス。
    :param content: ファイルに書き込む内容。
    """
    ensure_directory(path.parent)
    path.write_text(content, encoding="utf-8")


def generate_profile(build_dir: Path, profile_name: str | None = None) -> str:
    """
    指定されたプロファイルから CMake 設定と C ソースを生成する。

    :param build_dir: 生成物の出力先ビルドディレクトリ。
    :param profile_name: 使用するプロファイル名。None の場合はアクティブなプロファイルを使用する。
    :return: 使用したプロファイル名。
    """

    selected_profile = profile_name or require_active_profile_name()
    config = load_profile_config(selected_profile)
    settings_dir = generated_settings_dir(build_dir)
    users_c = profile_users_c_path(selected_profile).resolve().as_posix()

    context = {
        "profile_name": selected_profile,
        "cmake": config.cmake,
        "board": config.board,
        "keymap": config.keymap,
        "settings": config.settings,
        "layout": padded_layout(config.board.layout, config.board.key_capacity),
        "generated_src_dir": generated_src_dir(build_dir).resolve().as_posix(),
        "settings_dir": settings_dir.resolve().as_posix(),
        "users_c_path": users_c,
    }

    write_generated_file(
        settings_dir / "board.h",
        render_template("board.h.j2", context),
    )
    write_generated_file(
        settings_dir / "board.c",
        render_template("board.c.j2", context),
    )
    write_generated_file(
        settings_dir / "keymap.h",
        render_template("keymap.h.j2", context),
    )
    write_generated_file(
        settings_dir / "keymap.c",
        render_template("keymap.c.j2", context),
    )
    write_generated_file(
        settings_dir / "settings.h",
        render_template("settings.h.j2", context),
    )
    write_generated_file(
        settings_dir / "settings.c",
        render_template("settings.c.j2", context),
    )
    write_generated_file(
        generated_profile_cmake_path(build_dir),
        render_template("profile.cmake.j2", context),
    )

    return selected_profile


def register_profile_commands(app: typer.Typer) -> None:
    @app.command("profile", help="PWMK のアクティブプロファイルを切り替える。")
    def profile_command(
        profile_name: Annotated[
            str | None,
            typer.Argument(help="切り替えるプロファイル名。"),
        ] = None,
        clear: Annotated[
            bool,
            typer.Option("--clear", help="現在のプロファイル選択を解除する。"),
        ] = False,
    ) -> int:
        if clear:
            if profile_name is not None:
                raise typer.BadParameter(
                    " `--clear` を使用する場合、プロファイル名を指定できません。"
                )
            clear_profile_selection()
            return 0

        if profile_name is None:
            raise typer.BadParameter("プロファイル名が指定されていません。")

        select_profile(profile_name)
        return 0

    @app.command(
        "generate", help="アクティブプロファイルから CMake 設定と C ソースを生成する。"
    )
    def generate_command(
        build_dir: Annotated[
            Path,
            typer.Option(help="生成物の出力先ビルドディレクトリ。"),
        ] = Path("build"),
    ) -> int:
        generate_profile(build_dir.resolve())
        return 0
