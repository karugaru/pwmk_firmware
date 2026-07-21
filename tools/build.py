from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_SDK_TAG = "2.3.0"  # pico-sdk の git タグの既定値
# ビルドに必要なパッケージ
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


def parse_args() -> argparse.Namespace:
    """
    引数を解析する。

    :return: 解析結果の argparse.Namespace オブジェクト
    """

    parser = argparse.ArgumentParser(description="PWMK ファームウェアをビルドする。")
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

    return parser.parse_args()


def repo_root() -> Path:
    """
    リポジトリのルートディレクトリを返す。

    :return: リポジトリのルートディレクトリの Path オブジェクト
    """

    return Path(__file__).resolve().parents[1]


def run(
    command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None
) -> None:
    """
    指定されたコマンドを実行する。

    :param command: 実行するコマンドのリスト
    :param cwd: コマンドを実行するカレントディレクトリ
    :param env: コマンド実行時の環境変数
    :raises subprocess.CalledProcessError: コマンドの実行が失敗した場合
    """

    printable = " ".join(command)
    print(f"+ {printable}")
    subprocess.run(command, cwd=cwd, env=env, check=True)


def completed_process(command: list[str]) -> subprocess.CompletedProcess[str]:
    """
    指定されたコマンドを実行し、CompletedProcess オブジェクトを返す。

    :param command: 実行するコマンドのリスト
    :return: subprocess.CompletedProcess オブジェクト
    """

    return subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
    )


def ensure_command(name: str) -> None:
    """
    指定されたコマンドが存在することを確認する。存在しない場合は SystemExit を発生させる。

    :param name: 確認するコマンドの名前
    :raises SystemExit: コマンドが存在しない場合
    """

    if shutil.which(name) is None:
        raise SystemExit(f"次の必要なコマンドが見つかりません: {name}")


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


def build(args: argparse.Namespace) -> None:
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

    env = os.environ.copy()
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
    ]

    if args.sdk_path:
        cmake_args.append(f"-DPICO_SDK_PATH={Path(args.sdk_path).resolve()}")
    else:
        sdk_fetch_dir = build_dir / "pico-sdk"
        cmake_args.extend(
            [
                "-DPICO_SDK_FETCH_FROM_GIT=ON",
                f"-DPICO_SDK_FETCH_FROM_GIT_TAG={args.sdk_tag}",
                f"-DPICO_SDK_FETCH_FROM_GIT_PATH={sdk_fetch_dir}",
            ]
        )
        env["PICO_SDK_FETCH_FROM_GIT"] = "ON"
        env["PICO_SDK_FETCH_FROM_GIT_TAG"] = args.sdk_tag
        env["PICO_SDK_FETCH_FROM_GIT_PATH"] = str(sdk_fetch_dir)

    run(cmake_args, cwd=source_dir, env=env)
    run(
        ["cmake", "--build", str(build_dir), "--target", args.target],
        cwd=source_dir,
        env=env,
    )


def main() -> int:
    """
    メイン関数。

    :return: 終了ステータスコード
    """
    args = parse_args()

    if platform.system() != "Linux":
        raise SystemExit("このスクリプトはLinux 環境で実行してください。")

    build(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
