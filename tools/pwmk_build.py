from __future__ import annotations

import argparse

from pwmk_build_prep import (
    DEFAULT_PICOTOOL_TAG,
    DEFAULT_SDK_TAG,
    prepare_build_environment,
)
from pwmk_common import ensure_linux, run


def add_build_subcommand(
    subparsers: argparse._SubParsersAction[argparse.ArgumentParser],
) -> None:
    """
    build サブコマンドを登録する。

    :param subparsers: サブコマンド登録先
    """

    parser = subparsers.add_parser("build", help="PWMK ファームウェアをビルドする。")
    parser.add_argument(
        "--build-dir",
        default="build/cli",
        help="ビルド成果物の出力ディレクトリ。",
    )
    parser.add_argument(
        "--sdk-tag",
        default=DEFAULT_SDK_TAG,
        help="pico-sdk を git から取得する際に使用するタグ。",
    )
    parser.add_argument(
        "--sdk-path",
        help="pico-sdk のパス。指定しない場合は自動的に git から取得する。",
    )
    parser.add_argument(
        "--picotool-tag",
        default=DEFAULT_PICOTOOL_TAG,
        help="picotool を git から取得する際に使用するタグ。",
    )
    parser.add_argument(
        "--picotool-path",
        help="picotool のパス。指定しない場合は自動的に git から取得する。",
    )
    parser.add_argument(
        "--enable-usb",
        choices=("ON", "OFF"),
        default="ON",
        help="PWMK_ENABLE_USB に設定する値。",
    )
    parser.add_argument(
        "--enable-ble",
        choices=("ON", "OFF"),
        default="ON",
        help="PWMK_ENABLE_BLE に設定する値。",
    )
    parser.add_argument(
        "--build-type",
        default="Release",
        help="CMAKE_BUILD_TYPE に設定する値。",
    )
    parser.add_argument(
        "--target",
        default="pwmk",
        help="cmake --build に渡すビルドターゲット。",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="ビルド前にビルドディレクトリを削除する。",
    )
    parser.add_argument(
        "--skip-deps",
        action="store_true",
        help="ビルド依存関係の自動インストールをスキップする。",
    )
    parser.add_argument(
        "--delete-cached-repos",
        action="store_true",
        help=(
            "自動取得した pico-sdk / picotool のキャッシュを削除してからビルドする。"
        ),
    )
    parser.set_defaults(handler=handle_build_command)


def run_build(args: argparse.Namespace) -> None:
    """
    ビルドを実行する。

    :param args: コマンドライン引数
    """

    preparation = prepare_build_environment(args)

    cmake_args = [
        "cmake",
        "-S",
        str(preparation.source_dir),
        "-B",
        str(preparation.build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_BUILD_TYPE={args.build_type}",
        f"-DPWMK_ENABLE_USB={args.enable_usb}",
        f"-DPWMK_ENABLE_BLE={args.enable_ble}",
        f"-DPICO_SDK_PATH={preparation.sdk_dir}",
        f"-DPICOTOOL_FETCH_FROM_GIT_PATH={preparation.picotool_dir.parent}",
        f"-Dpicotool_DIR={preparation.picotool_dir}",
    ]

    run(cmake_args, cwd=preparation.source_dir, env=preparation.env)
    run(
        ["cmake", "--build", str(preparation.build_dir), "--target", args.target],
        cwd=preparation.source_dir,
        env=preparation.env,
    )


def handle_build_command(args: argparse.Namespace) -> int:
    """
    build サブコマンドを実行する。

    :param args: コマンドライン引数
    :return: 終了ステータスコード
    """

    ensure_linux()
    run_build(args)
    return 0
