from __future__ import annotations

import json
import lzma
import shutil
from pathlib import Path
from typing import Annotated, Any

import typer
import yaml
from jinja2 import Environment, FileSystemLoader, StrictUndefined
from pwmk_common import ensure_directory, repo_root
from pwmk_profile_schema import ProfileConfig
from pydantic import ValidationError

USERS_DIR_NAME = "users"
CURRENT_PROFILE_FILE_NAME = ".current_profile"
PROFILE_FILE_NAME = "profile.yaml"
KEYBOARD_LAYOUT_FILE_NAME = "keyboard-layout.json"

KeyboardLayout = list[list[dict[str, Any] | str]]


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
    return profile_dir(profile_name) / PROFILE_FILE_NAME


def keyboard_layout_path(profile_name: str) -> Path:
    """指定されたユーザープロファイルのキーボードレイアウト JSON のパスを返す。"""
    return profile_dir(profile_name) / KEYBOARD_LAYOUT_FILE_NAME


def profile_c_sources(profile_name: str) -> list[Path]:
    """指定されたユーザープロファイル配下の C ソースファイル一覧を返す。"""
    return sorted(profile_dir(profile_name).glob("*.c"))


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
    environment.filters["c_hex"] = c_hex_literal
    return environment


def c_float_literal(value: float) -> str:
    """浮動小数点数を C の浮動小数点リテラルとして返す。"""
    text = f"{value:g}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return f"{text}f"


def c_hex_literal(value: int) -> str:
    """整数を C の 16 ビット 16 進数リテラルとして返す。"""
    return f"0x{value:04X}"


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
    if not yaml_path.exists():
        missing = yaml_path.relative_to(repo_root())
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
            f"{profile_name} のトップレベルはマッピングである必要があります。"
        )

    try:
        return ProfileConfig.model_validate(data)
    except ValidationError as error:
        raise SystemExit(str(error)) from error


def load_keyboard_layout(profile_name: str) -> KeyboardLayout:
    """指定されたプロファイルの Vial 用キーボードレイアウトを読み込む。"""
    path = keyboard_layout_path(profile_name)
    if not path.exists():
        missing = path.relative_to(repo_root())
        raise SystemExit(f"{KEYBOARD_LAYOUT_FILE_NAME} が見つかりません: {missing}")

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        relative_path = path.relative_to(repo_root())
        raise SystemExit(
            f"{KEYBOARD_LAYOUT_FILE_NAME} の JSON が不正です: {relative_path}: {error.msg}"
        ) from error

    if not isinstance(data, list):
        raise SystemExit(
            f"{KEYBOARD_LAYOUT_FILE_NAME} のトップレベルは配列である必要があります。"
        )

    layout: KeyboardLayout = []
    for row_index, row in enumerate(data):
        if not isinstance(row, list):
            raise SystemExit(
                f"{KEYBOARD_LAYOUT_FILE_NAME} の行 {row_index} は配列である必要があります。"
            )

        layout_row: list[dict[str, Any] | str] = []
        for item in row:
            if not isinstance(item, (dict, str)):
                raise SystemExit(
                    f"{KEYBOARD_LAYOUT_FILE_NAME} の各要素は文字列またはオブジェクトである必要があります。"
                )
            layout_row.append(item)
        layout.append(layout_row)

    return layout


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


def vial_unlock_combo(config: ProfileConfig) -> list[tuple[int, int]]:
    """Vial の解除に使用する物理キー座標を返す。"""
    if config.board.vial_unlock_combo is not None:
        return list(config.board.vial_unlock_combo)

    positions = [
        (row, col) for row, col in config.board.layout if row >= 0 and col >= 0
    ]
    if not positions:
        positions = [(0, 0)]

    return [positions[min(index, len(positions) - 1)] for index in range(3)]


def vial_keyboard_definition_json(
    config: ProfileConfig, keymap_layout: KeyboardLayout
) -> str:
    """プロファイルと keyboard-layout.json から Vial keyboard definition を生成する。"""
    definition = {
        "name": config.settings.device_name,
        "vendorId": f"0x{config.settings.usb_vid:04X}",
        "productId": f"0x{config.settings.usb_pid:04X}",
        "lighting": "none",
        "matrix": {"rows": config.board.rows, "cols": config.board.cols},
        "layouts": {"keymap": keymap_layout or [[]]},
    }
    return json.dumps(definition, ensure_ascii=True, indent=2)


def vial_keyboard_definition(
    config: ProfileConfig, keymap_layout: KeyboardLayout
) -> bytes:
    """プロファイルから Vial が解釈できる LZMA 圧縮定義を生成する。"""
    return lzma.compress(
        vial_keyboard_definition_json(config, keymap_layout).encode("utf-8"),
        format=lzma.FORMAT_ALONE,
    )


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


def generated_device_id_header_path(build_dir: Path) -> Path:
    """ビルドディレクトリ内の生成されたデバイス名ヘッダーのパスを返す。"""
    return generated_src_dir(build_dir) / "device_identity.h"


def generated_gatt_path(build_dir: Path) -> Path:
    """ビルドディレクトリ内の生成された GATT 定義ファイルのパスを返す。"""
    return generated_profile_root(build_dir) / "pwmk.gatt"


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
    keymap_layout = load_keyboard_layout(selected_profile)
    settings_dir = generated_settings_dir(build_dir)
    profile_sources = [
        path.resolve().as_posix() for path in profile_c_sources(selected_profile)
    ]

    device_name = config.settings.device_name
    manufacturer_name = config.settings.manufacturer_name
    vial_definition_json = vial_keyboard_definition_json(config, keymap_layout)

    context = {
        "profile_name": selected_profile,
        "cmake": config.cmake,
        "board": config.board,
        "keymap": config.keymap,
        "settings": config.settings,
        "layout": padded_layout(config.board.layout, config.board.key_capacity),
        "vial_keyboard_definition": vial_keyboard_definition(config, keymap_layout),
        "vial_unlock_combo": vial_unlock_combo(config),
        "generated_src_dir": generated_src_dir(build_dir).resolve().as_posix(),
        "settings_dir": settings_dir.resolve().as_posix(),
        "profile_c_sources": profile_sources,
        "device_name": device_name,
        "manufacturer_name": manufacturer_name,
    }

    write_generated_file(
        settings_dir / "board.h",
        render_template("board.h.j2", context),
    )
    write_generated_file(
        settings_dir / "keymap.h",
        render_template("keymap.h.j2", context),
    )
    write_generated_file(
        settings_dir / "vial_definition.h",
        render_template("vial_definition.h.j2", context),
    )
    write_generated_file(
        settings_dir / "vial.json",
        vial_definition_json,
    )
    write_generated_file(
        settings_dir / "settings.h",
        render_template("settings.h.j2", context),
    )
    write_generated_file(
        generated_profile_cmake_path(build_dir),
        render_template("profile.cmake.j2", context),
    )
    write_generated_file(
        generated_device_id_header_path(build_dir),
        render_template("device_identity.h.j2", context),
    )
    write_generated_file(
        generated_gatt_path(build_dir),
        render_template("pwmk.gatt.j2", context),
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
