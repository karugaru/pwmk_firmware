from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_SDK_TAG = "2.3.0"  # pico-sdk の git タグの既定値
DEFAULT_PICOTOOL_TAG = "2.3.0"  # picotool の git タグの既定値
PICO_SDK_REPOSITORY_URL = "https://github.com/raspberrypi/pico-sdk.git"
PICOTOOL_REPOSITORY_URL = "https://github.com/raspberrypi/picotool.git"
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


def capture_output(
    command: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None
) -> str:
    """
    指定されたコマンドを実行し、標準出力を文字列として返す。

    :param command: 実行するコマンドのリスト
    :param cwd: コマンドを実行するカレントディレクトリ
    :param env: コマンド実行時の環境変数
    :return: 標準出力
    """

    printable = " ".join(command)
    print(f"+ {printable}")
    result = subprocess.run(
        command,
        cwd=cwd,
        env=env,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


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


def ensure_directory(path: Path) -> None:
    """
    指定されたディレクトリが存在することを確認し、存在しなければ作成する。

    :param path: 作成対象ディレクトリ
    """

    path.mkdir(parents=True, exist_ok=True)


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


def resolve_remote_ref(
    repository_url: str,
    ref_name: str,
    *,
    env: dict[str, str],
) -> str:
    """
    リモート上の ref が指す commit hash を返す。

    :param repository_url: 対象リポジトリ URL
    :param ref_name: 解決したい ref 名
    :param env: 実行環境変数
    :return: ref が指す commit hash
    :raises SystemExit: ref が見つからない場合
    """

    output = capture_output(
        ["git", "ls-remote", repository_url, ref_name, f"refs/tags/{ref_name}"],
        env=env,
    )
    for line in output.splitlines():
        stripped = line.strip()
        if not stripped:
            continue

        revision, _, resolved_ref = stripped.partition("\t")
        if resolved_ref == f"refs/tags/{ref_name}^{{}}":
            return revision
        if resolved_ref in {
            ref_name,
            f"refs/heads/{ref_name}",
            f"refs/tags/{ref_name}",
        }:
            fallback_revision = revision

    if "fallback_revision" in locals():
        return fallback_revision

    raise SystemExit(f"指定された git ref が見つかりません: {ref_name}")


def clone_or_update_repo(
    repository_url: str,
    destination: Path,
    ref_name: str,
    *,
    env: dict[str, str],
) -> Path:
    """
    リポジトリを clone または更新して指定 ref に揃える。

    :param repository_url: clone 元 URL
    :param destination: clone 先ディレクトリ
    :param ref_name: checkout する ref 名
    :param env: 実行環境変数
    :return: 利用可能なリポジトリディレクトリ
    """

    parent = destination.parent
    ensure_directory(parent)

    desired_revision = resolve_remote_ref(repository_url, ref_name, env=env)
    git_dir = destination / ".git"
    if not git_dir.exists():
        if destination.exists():
            shutil.rmtree(destination)
        run(
            [
                "git",
                "clone",
                "--branch",
                ref_name,
                "--depth",
                "1",
                repository_url,
                str(destination),
            ],
            env=env,
        )
        run(
            ["git", "submodule", "update", "--init", "--recursive"],
            cwd=destination,
            env=env,
        )
        return destination

    run(
        ["git", "remote", "set-url", "origin", repository_url], cwd=destination, env=env
    )
    if current_git_head(destination, env=env) != desired_revision:
        shutil.rmtree(destination)
        return clone_or_update_repo(
            repository_url,
            destination,
            ref_name,
            env=env,
        )

    run(
        ["git", "submodule", "update", "--init", "--recursive"],
        cwd=destination,
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

    return clone_or_update_repo(
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

    return clone_or_update_repo(
        PICOTOOL_REPOSITORY_URL,
        source_dir,
        args.picotool_tag,
        env=env,
    )


def current_git_head(repo_dir: Path, *, env: dict[str, str]) -> str:
    """
    現在 checkout されている revision 識別子を返す。

    :param repo_dir: 対象リポジトリ
    :param env: 実行環境変数
    :return: commit hash またはローカルパス識別子
    """

    if not (repo_dir / ".git").exists():
        return f"path:{repo_dir.resolve()}"

    return capture_output(["git", "rev-parse", "HEAD"], cwd=repo_dir, env=env)


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


def picotool_stamp_path(picotool_tag: str) -> Path:
    """
    picotool のビルド内容を表すスタンプファイルのパスを返す。

    :param picotool_tag: picotool の git タグ
    :return: スタンプファイルのパス
    """

    return picotool_cache_dir(picotool_tag) / "install" / ".build-stamp"


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
    stamp_path = picotool_stamp_path(args.picotool_tag).resolve()

    run(
        [
            "cmake",
            "--version",
        ],
        env=env,
    )

    sdk_dir = ensure_sdk_source_dir(args, env=env)
    picotool_src_dir = ensure_picotool_source_dir(args, env=env)
    source_head = current_git_head(picotool_src_dir, env=env)
    sdk_head = current_git_head(sdk_dir, env=env)
    stamp_value = f"picotool={source_head}\npico-sdk={sdk_head}\n"

    if picotool_is_installed(args.picotool_tag) and stamp_path.exists():
        if stamp_path.read_text(encoding="utf-8") == stamp_value:
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

    stamp_path.write_text(stamp_value, encoding="utf-8")
    return config_dir


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
    ensure_command("git")

    env = os.environ.copy()
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
