from __future__ import annotations

import argparse
import os
import shutil
from pathlib import Path

from pwmk_common import (
    completed_process,
    ensure_command,
    ensure_directory,
    ensure_linux,
    repo_root,
    run,
)

DEFAULT_SDK_TAG = "2.3.0"
DEFAULT_PICOTOOL_TAG = "2.3.0"
PICO_SDK_REPOSITORY_URL = "https://github.com/raspberrypi/pico-sdk.git"
PICOTOOL_REPOSITORY_URL = "https://github.com/raspberrypi/picotool.git"
APT_PACKAGES = [
    "build-essential",
    "ca-certificates",
    "cmake",
    "gcc-arm-none-eabi",
    "git",
    "libnewlib-arm-none-eabi",
    "libstdc++-arm-none-eabi-newlib",
    "ninja-build",
]


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


def cache_root() -> Path:
    """
    外部依存物のキャッシュルートディレクトリを返す。

    :return: キャッシュルートディレクトリ
    """

    return Path.home() / ".pwmk"


def sanitized_tag(tag: str) -> str:
    """
    キャッシュディレクトリ名に使えるようタグ文字列をサニタイズする。

    :param tag: サニタイズ対象のタグ文字列
    :return: サニタイズ済み文字列
    """

    return "".join(
        character if character.isalnum() or character in ("-", "_", ".") else "_"
        for character in tag
    )


def sdk_cache_dir(sdk_tag: str) -> Path:
    """
    pico-sdk を git から取得する際のキャッシュディレクトリを返す。

    :param sdk_tag: SDK の git タグ
    :return: SDK キャッシュディレクトリの Path オブジェクト
    """

    return cache_root() / f"pico-sdk-{sanitized_tag(sdk_tag)}"


def picotool_cache_dir(picotool_tag: str) -> Path:
    """
    picotool を導入するキャッシュディレクトリを返す。

    :param picotool_tag: picotool の git タグ
    :return: picotool キャッシュディレクトリの Path オブジェクト
    """

    return cache_root() / f"picotool-{sanitized_tag(picotool_tag)}"


def package_is_installed(package_name: str) -> bool:
    """
    指定されたパッケージがインストールされているかどうかを確認する。

    :param package_name: 確認するパッケージの名前
    :return: パッケージがインストールされている場合は True、それ以外の場合は False
    """

    result = completed_process(["dpkg-query", "-W", "-f=${Status}", package_name])
    return result.returncode == 0 and "install ok installed" in result.stdout


def privileged_prefix() -> list[str]:
    """
    root 権限が必要なコマンドを実行するためのプレフィックスを返す。

    :return: root 権限が必要なコマンドを実行するためのプレフィックスのリスト
    :raises SystemExit: sudo が使えない場合
    """

    geteuid = getattr(os, "geteuid", None)
    if geteuid is not None and geteuid() == 0:
        return []

    ensure_command("sudo")
    return ["sudo"]


def install_dependencies() -> None:
    """
    ビルドに必要な依存パッケージを導入する。すでに導入済みのパッケージはスキップされる。
    """

    ensure_command("apt-get")
    ensure_command("dpkg-query")

    missing_packages = [
        package_name
        for package_name in APT_PACKAGES
        if not package_is_installed(package_name)
    ]
    if not missing_packages:
        return

    prefix = privileged_prefix()
    run(prefix + ["apt-get", "update"])
    run(
        prefix
        + [
            "apt-get",
            "install",
            "-y",
            "--no-install-recommends",
            *missing_packages,
        ]
    )


def clone_repo_if_needed(
    repository_url: str,
    destination: Path,
    ref_name: str,
    *,
    env: dict[str, str],
) -> Path:
    """
    リポジトリが未取得の場合だけ clone する。

    :param repository_url: clone 元 URL
    :param destination: clone 先ディレクトリ
    :param ref_name: checkout する ref 名
    :param env: 実行環境変数
    :return: 利用可能なリポジトリディレクトリ
    """

    ensure_directory(destination.parent)

    if destination.exists():
        if not (destination / ".git").exists():
            raise SystemExit(
                f"キャッシュディレクトリが git リポジトリではありません: {destination}"
            )
        return destination

    run(
        [
            "git",
            "clone",
            "--branch",
            ref_name,
            "--depth",
            "1",
            "--recursive",
            repository_url,
            str(destination),
        ],
        env=env,
    )
    return destination


def picotool_config_dir(picotool_tag: str) -> Path:
    """
    picotool の CMake package config が配置されるディレクトリを返す。

    :param picotool_tag: picotool の git タグ
    :return: picotoolConfig.cmake を含むディレクトリの Path オブジェクト
    """

    return picotool_cache_dir(picotool_tag) / "install" / "picotool"


def picotool_is_installed(picotool_tag: str) -> bool:
    """
    指定タグの picotool がキャッシュ済みかどうかを返す。

    :param picotool_tag: picotool の git タグ
    :return: picotool が導入済みなら True
    """

    config_dir = picotool_config_dir(picotool_tag)
    return (config_dir / "picotoolConfig.cmake").exists() and (
        config_dir / "picotool"
    ).exists()


def sdk_source_dir(args: argparse.Namespace) -> Path:
    """
    picotool ビルド時に参照する pico-sdk のソースディレクトリを返す。

    :param args: コマンドライン引数
    :return: pico-sdk ソースディレクトリ
    """

    if args.sdk_path:
        return Path(args.sdk_path).resolve()

    return sdk_cache_dir(args.sdk_tag) / "src"


def picotool_source_dir(args: argparse.Namespace) -> Path:
    """
    picotool ソースディレクトリを返す。

    :param args: コマンドライン引数
    :return: picotool ソースディレクトリ
    """

    if args.picotool_path:
        return Path(args.picotool_path).resolve()

    return picotool_cache_dir(args.picotool_tag) / "src"


def ensure_sdk_source_dir(args: argparse.Namespace, *, env: dict[str, str]) -> Path:
    """
    pico-sdk ソースを確保する。

    :param args: コマンドライン引数
    :param env: 実行環境変数
    :return: 利用可能な pico-sdk ソースディレクトリ
    """

    source_dir = sdk_source_dir(args)
    if args.sdk_path:
        if not source_dir.exists():
            raise SystemExit(f"指定された pico-sdk が見つかりません: {source_dir}")
        return source_dir

    return clone_repo_if_needed(
        PICO_SDK_REPOSITORY_URL,
        source_dir,
        args.sdk_tag,
        env=env,
    )


def ensure_picotool_source_dir(
    args: argparse.Namespace, *, env: dict[str, str]
) -> Path:
    """
    picotool ソースを確保する。

    :param args: コマンドライン引数
    :param env: 実行環境変数
    :return: 利用可能な picotool ソースディレクトリ
    """

    source_dir = picotool_source_dir(args)
    if args.picotool_path:
        if not source_dir.exists():
            raise SystemExit(f"指定された picotool が見つかりません: {source_dir}")
        run(
            ["git", "submodule", "update", "--init", "--recursive"],
            cwd=source_dir,
            env=env,
        )
        return source_dir

    return clone_repo_if_needed(
        PICOTOOL_REPOSITORY_URL,
        source_dir,
        args.picotool_tag,
        env=env,
    )


def picotool_build_dir(picotool_tag: str) -> Path:
    """
    picotool のビルドディレクトリを返す。

    :param picotool_tag: picotool の git タグ
    :return: ビルドディレクトリ
    """

    return picotool_cache_dir(picotool_tag) / "build"


def picotool_install_dir(picotool_tag: str) -> Path:
    """
    picotool の install ディレクトリを返す。

    :param picotool_tag: picotool の git タグ
    :return: install ディレクトリ
    """

    return picotool_cache_dir(picotool_tag) / "install"


def ensure_picotool(
    args: argparse.Namespace,
    *,
    env: dict[str, str],
) -> Path:
    """
    Linux 側キャッシュ配下に picotool を導入し、CMake package config のディレクトリを返す。

    pico-setup.sh と同様に git clone して cmake で build/install する。
    install 先は専用キャッシュ配下に固定し、main build ではその picotool_DIR を明示する。

    :param args: コマンドライン引数
    :param env: 実行環境変数
    :return: picotoolConfig.cmake を含むディレクトリ
    """

    config_dir = picotool_config_dir(args.picotool_tag).resolve()
    install_dir = picotool_install_dir(args.picotool_tag).resolve()
    build_dir = picotool_build_dir(args.picotool_tag).resolve()

    run(["cmake", "--version"], env=env)

    sdk_dir = ensure_sdk_source_dir(args, env=env)
    picotool_src_dir = ensure_picotool_source_dir(args, env=env)

    if picotool_is_installed(args.picotool_tag):
        return config_dir

    if build_dir.exists():
        shutil.rmtree(build_dir)
    if install_dir.exists():
        shutil.rmtree(install_dir)
    ensure_directory(build_dir)
    ensure_directory(install_dir)

    run(
        [
            "cmake",
            "-S",
            str(picotool_src_dir),
            "-B",
            str(build_dir),
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_INSTALL_PREFIX={install_dir}",
            "-DPICOTOOL_FLAT_INSTALL=1",
            "-DPICOTOOL_NO_LIBUSB=1",
            f"-DPICO_SDK_PATH={sdk_dir}",
        ],
        env=env,
    )
    run(["cmake", "--build", str(build_dir)], env=env)
    run(["cmake", "--install", str(build_dir)], env=env)

    if not picotool_is_installed(args.picotool_tag):
        raise SystemExit("picotool の導入に失敗しました。")

    return config_dir


def delete_cached_repos(args: argparse.Namespace) -> None:
    """
    自動取得した SDK / picotool キャッシュを削除する。

    :param args: コマンドライン引数
    """

    if args.sdk_path is None:
        sdk_dir = sdk_cache_dir(args.sdk_tag)
        if sdk_dir.exists():
            shutil.rmtree(sdk_dir)

    if args.picotool_path is None:
        picotool_dir = picotool_cache_dir(args.picotool_tag)
        if picotool_dir.exists():
            shutil.rmtree(picotool_dir)


def run_build(args: argparse.Namespace) -> None:
    """
    ビルドを実行する。

    :param args: コマンドライン引数
    """

    source_dir = repo_root()
    build_dir = (source_dir / args.build_dir).resolve()

    if not args.skip_deps:
        install_dependencies()

    if args.clean and build_dir.exists():
        shutil.rmtree(build_dir)

    build_dir.mkdir(parents=True, exist_ok=True)

    ensure_command("cmake")
    ensure_command("ninja")
    ensure_command("git")

    env = os.environ.copy()
    if args.delete_cached_repos:
        delete_cached_repos(args)

    sdk_dir = ensure_sdk_source_dir(args, env=env)
    picotool_dir = ensure_picotool(args, env=env)

    cmake_args = [
        "cmake",
        "-S",
        str(source_dir),
        "-B",
        str(build_dir),
        "-G",
        "Ninja",
        f"-DCMAKE_BUILD_TYPE={args.build_type}",
        f"-DPWMK_ENABLE_USB={args.enable_usb}",
        f"-DPWMK_ENABLE_BLE={args.enable_ble}",
        f"-DPICO_SDK_PATH={sdk_dir}",
        f"-DPICOTOOL_FETCH_FROM_GIT_PATH={picotool_dir.parent}",
        f"-Dpicotool_DIR={picotool_dir}",
    ]

    env["PICO_SDK_PATH"] = str(sdk_dir)
    env["PICOTOOL_FETCH_FROM_GIT_PATH"] = str(picotool_dir.parent)
    env["picotool_DIR"] = str(picotool_dir)

    run(cmake_args, cwd=source_dir, env=env)
    run(
        ["cmake", "--build", str(build_dir), "--target", args.target],
        cwd=source_dir,
        env=env,
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
