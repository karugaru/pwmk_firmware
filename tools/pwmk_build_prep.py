from __future__ import annotations

import os
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Literal

from pwmk_common import (
    completed_process,
    ensure_command,
    ensure_directory,
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
DNF_PACKAGES = [
    "ca-certificates",
    "cmake",
    "gcc",
    "gcc-c++",
    "git",
    "make",
    "ninja-build",
    "arm-none-eabi-gcc-cs",
    "arm-none-eabi-gcc-cs-c++",
    "arm-none-eabi-newlib",
]
PACMAN_PACKAGES = [
    "base-devel",
    "ca-certificates",
    "cmake",
    "git",
    "ninja",
    "arm-none-eabi-gcc",
    "arm-none-eabi-newlib",
]


@dataclass(frozen=True)
class BuildCommandArgs:
    """
    build サブコマンドの入力値を保持する。

    :param build_dir: ビルド成果物の出力ディレクトリ
    :param sdk_tag: pico-sdk を取得する git タグ
    :param sdk_path: 既存 pico-sdk のパス
    :param picotool_tag: picotool を取得する git タグ
    :param picotool_path: 既存 picotool のパス
    :param enable_usb: PWMK_ENABLE_USB の設定値
    :param enable_ble: PWMK_ENABLE_BLE の設定値
    :param build_type: CMAKE_BUILD_TYPE の設定値
    :param target: cmake --build に渡すターゲット
    :param clean: ビルド前に出力ディレクトリを削除するかどうか
    :param skip_deps: 依存関係の自動インストールをスキップするかどうか
    :param delete_cached_repos: 依存キャッシュを削除してからビルドするかどうか
    """

    build_dir: str = "build/cli"
    sdk_tag: str = DEFAULT_SDK_TAG
    sdk_path: Path | None = None
    picotool_tag: str = DEFAULT_PICOTOOL_TAG
    picotool_path: Path | None = None
    enable_usb: Literal["ON", "OFF"] = "ON"
    enable_ble: Literal["ON", "OFF"] = "ON"
    build_type: str = "Release"
    target: str = "pwmk"
    clean: bool = False
    skip_deps: bool = False
    delete_cached_repos: bool = False


@dataclass(frozen=True)
class PackageManager:
    """
    依存パッケージ導入に使うパッケージマネージャー定義。

    :param name: 識別名
    :param command: パッケージマネージャー実行コマンド
    :param query_command: パッケージ導入済み確認コマンドを組み立てる関数
    :param install_command: パッケージ導入コマンドを組み立てる関数
    :param packages: 導入対象パッケージ一覧
    :param update_command: 必要なら導入前に実行する更新コマンド
    """

    name: str
    command: str
    query_command: Callable[[str], list[str]]
    install_command: Callable[[list[str]], list[str]]
    packages: list[str]
    update_command: list[str] | None = None


@dataclass(frozen=True)
class BuildPreparation:
    """
    ビルド前準備で確定したパスと環境変数を保持する。

    :param source_dir: ファームウェアソースルート
    :param build_dir: ビルド出力ディレクトリ
    :param env: ビルドに使う環境変数
    :param sdk_dir: 利用する pico-sdk ソースディレクトリ
    :param picotool_dir: 利用する picotool package config ディレクトリ
    """

    source_dir: Path
    build_dir: Path
    env: dict[str, str]
    sdk_dir: Path
    picotool_dir: Path


PACKAGE_MANAGERS = [
    PackageManager(
        name="apt",
        command="apt-get",
        query_command=lambda package_name: [
            "dpkg-query",
            "-W",
            "-f=${Status}",
            package_name,
        ],
        install_command=lambda packages: [
            "apt-get",
            "install",
            "-y",
            "--no-install-recommends",
            *packages,
        ],
        packages=APT_PACKAGES,
        update_command=["apt-get", "update"],
    ),
    PackageManager(
        name="dnf",
        command="dnf",
        query_command=lambda package_name: ["rpm", "-q", package_name],
        install_command=lambda packages: ["dnf", "install", "-y", *packages],
        packages=DNF_PACKAGES,
    ),
    PackageManager(
        name="pacman",
        command="pacman",
        query_command=lambda package_name: ["pacman", "-Q", package_name],
        install_command=lambda packages: [
            "pacman",
            "-S",
            "--noconfirm",
            "--needed",
            *packages,
        ],
        packages=PACMAN_PACKAGES,
        update_command=["pacman", "-Sy", "--noconfirm"],
    ),
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


def detected_package_manager() -> PackageManager:
    """
    利用可能なパッケージマネージャーを検出して返す。

    :return: 利用可能なパッケージマネージャー定義
    :raises SystemExit: 対応するパッケージマネージャーが見つからない場合
    """

    for package_manager in PACKAGE_MANAGERS:
        if shutil.which(package_manager.command) is not None:
            return package_manager

    raise SystemExit(
        "対応するパッケージマネージャーが見つかりません。apt-get、dnf、pacman のいずれかが必要です。"
    )


def package_is_installed(package_manager: PackageManager, package_name: str) -> bool:
    """
    指定されたパッケージがインストールされているかどうかを確認する。

    :param package_manager: 利用するパッケージマネージャー定義
    :param package_name: 確認するパッケージの名前
    :return: パッケージがインストールされている場合は True、それ以外の場合は False
    """

    result = completed_process(package_manager.query_command(package_name))
    if package_manager.name == "apt":
        return result.returncode == 0 and "install ok installed" in result.stdout

    return result.returncode == 0


def linux_os_release() -> dict[str, str]:
    """
    /etc/os-release の内容をキーと値の辞書として返す。

    :return: os-release の内容
    """

    os_release_path = Path("/etc/os-release")
    if not os_release_path.exists():
        return {}

    entries: dict[str, str] = {}
    for line in os_release_path.read_text(encoding="utf-8").splitlines():
        if not line or "=" not in line:
            continue

        key, value = line.split("=", 1)
        entries[key] = value.strip().strip('"').strip("'")

    return entries


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


def prepare_dnf_repositories(prefix: list[str]) -> None:
    """
    AlmaLinux で必要な追加リポジトリを有効化する。

    :param prefix: root 権限コマンドのプレフィックス
    """

    os_release = linux_os_release()
    if os_release.get("ID") != "almalinux":
        return

    run(prefix + ["dnf", "install", "-y", "epel-release", "dnf-plugins-core"])
    run(prefix + ["dnf", "config-manager", "--set-enabled", "crb"])


def install_dependencies() -> None:
    """
    ビルドに必要な依存パッケージを導入する。すでに導入済みのパッケージはスキップされる。
    """

    package_manager = detected_package_manager()
    ensure_command(package_manager.command)
    if package_manager.name == "apt":
        ensure_command("dpkg-query")
    elif package_manager.name == "dnf":
        ensure_command("rpm")

    prefix = privileged_prefix()
    if package_manager.name == "dnf":
        prepare_dnf_repositories(prefix)

    missing_packages = [
        package_name
        for package_name in package_manager.packages
        if not package_is_installed(package_manager, package_name)
    ]
    if not missing_packages:
        return

    if package_manager.update_command is not None:
        run(prefix + package_manager.update_command)
    run(prefix + package_manager.install_command(missing_packages))


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


def sdk_source_dir(args: BuildCommandArgs) -> Path:
    """
    picotool ビルド時に参照する pico-sdk のソースディレクトリを返す。

    :param args: コマンドライン引数
    :return: pico-sdk ソースディレクトリ
    """

    if args.sdk_path:
        return args.sdk_path.resolve()

    return sdk_cache_dir(args.sdk_tag) / "src"


def picotool_source_dir(args: BuildCommandArgs) -> Path:
    """
    picotool ソースディレクトリを返す。

    :param args: コマンドライン引数
    :return: picotool ソースディレクトリ
    """

    if args.picotool_path:
        return args.picotool_path.resolve()

    return picotool_cache_dir(args.picotool_tag) / "src"


def ensure_sdk_source_dir(args: BuildCommandArgs, *, env: dict[str, str]) -> Path:
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
    args: BuildCommandArgs, *, env: dict[str, str]
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
    args: BuildCommandArgs,
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


def delete_cached_repos(args: BuildCommandArgs) -> None:
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


def prepare_build_environment(args: BuildCommandArgs) -> BuildPreparation:
    """
    ビルド前準備を実行し、ビルド本体に必要なパスと環境変数を返す。

    :param args: コマンドライン引数
    :return: ビルド前準備の結果
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

    env["PICO_SDK_PATH"] = str(sdk_dir)
    env["PICOTOOL_FETCH_FROM_GIT_PATH"] = str(picotool_dir.parent)
    env["picotool_DIR"] = str(picotool_dir)

    return BuildPreparation(
        source_dir=source_dir,
        build_dir=build_dir,
        env=env,
        sdk_dir=sdk_dir,
        picotool_dir=picotool_dir,
    )
