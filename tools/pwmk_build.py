from __future__ import annotations

from pathlib import Path
from typing import Annotated

import typer

from pwmk_build_prep import (
    BuildCommandArgs,
    DEFAULT_PICOTOOL_TAG,
    DEFAULT_SDK_TAG,
    prepare_build_environment,
)
from pwmk_common import ensure_linux, run


def register_build_command(app: typer.Typer) -> None:
    """
    build サブコマンドを登録する。

    :param app: サブコマンド登録先
    """

    @app.command("build", help="PWMK ファームウェアをビルドする。")
    def build_command(
        build_dir: Annotated[
            str,
            typer.Option(help="ビルド成果物の出力ディレクトリ。"),
        ] = "build/cli",
        sdk_tag: Annotated[
            str,
            typer.Option(help="pico-sdk を git から取得する際に使用するタグ。"),
        ] = DEFAULT_SDK_TAG,
        sdk_path: Annotated[
            Path | None,
            typer.Option(help="pico-sdk のパス。指定しない場合は自動的に git から取得する。"),
        ] = None,
        picotool_tag: Annotated[
            str,
            typer.Option(help="picotool を git から取得する際に使用するタグ。"),
        ] = DEFAULT_PICOTOOL_TAG,
        picotool_path: Annotated[
            Path | None,
            typer.Option(help="picotool のパス。指定しない場合は自動的に git から取得する。"),
        ] = None,
        enable_usb: Annotated[
            bool,
            typer.Option("--usb/--no-usb", help="USB 機能を有効化するかどうか。"),
        ] = True,
        enable_ble: Annotated[
            bool,
            typer.Option("--ble/--no-ble", help="BLE 機能を有効化するかどうか。"),
        ] = True,
        build_type: Annotated[
            str,
            typer.Option(help="CMAKE_BUILD_TYPE に設定する値。"),
        ] = "Release",
        target: Annotated[
            str,
            typer.Option(help="cmake --build に渡すビルドターゲット。"),
        ] = "pwmk",
        clean: Annotated[
            bool,
            typer.Option("--clean/--no-clean", help="ビルド前にビルドディレクトリを削除する。"),
        ] = False,
        skip_deps: Annotated[
            bool,
            typer.Option(
                "--skip-deps/--install-deps",
                help="ビルド依存関係の自動インストールをスキップする。",
            ),
        ] = False,
        delete_cached_repos: Annotated[
            bool,
            typer.Option(
                "--delete-cached-repos/--keep-cached-repos",
                help="自動取得した pico-sdk / picotool のキャッシュを削除してからビルドする。",
            ),
        ] = False,
    ) -> int:
        return handle_build_command(
            BuildCommandArgs(
                build_dir=build_dir,
                sdk_tag=sdk_tag,
                sdk_path=sdk_path,
                picotool_tag=picotool_tag,
                picotool_path=picotool_path,
                enable_usb="ON" if enable_usb else "OFF",
                enable_ble="ON" if enable_ble else "OFF",
                build_type=build_type,
                target=target,
                clean=clean,
                skip_deps=skip_deps,
                delete_cached_repos=delete_cached_repos,
            )
        )


def run_build(args: BuildCommandArgs) -> None:
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


def handle_build_command(args: BuildCommandArgs) -> int:
    """
    build サブコマンドを実行する。

    :param args: コマンドライン引数
    :return: 終了ステータスコード
    """

    ensure_linux()
    run_build(args)
    return 0
