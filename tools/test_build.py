from __future__ import annotations

import platform
import shutil
import subprocess
import sys
from pathlib import Path

IMAGE = "ubuntu:24.04"
SDK_TAG = "2.3.0"
BUILD_DIR = "/tmp/pwmk-build-test"
BUILD_TYPE = "Release"
TARGET = "pwmk"


def repo_root() -> Path:
    """
    リポジトリのルートディレクトリを返す。

    :return: リポジトリのルートディレクトリの Path オブジェクト
    """

    return Path(__file__).resolve().parents[1]


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


def docker_command_prefix() -> list[str]:
    """
    Docker コマンドのプレフィックスを返す。通常は ["docker"] だが、sudo が必要な場合は ["sudo", "-n", "docker"] となる。

    :return: Docker コマンドのリスト
    """

    ensure_command("docker")
    direct = ["docker"]
    if completed_process(direct + ["version"]).returncode == 0:
        return direct

    if shutil.which("sudo") is not None:
        sudo_direct = ["sudo", "-n", "docker"]
        if completed_process(sudo_direct + ["version"]).returncode == 0:
            return sudo_direct

    raise SystemExit(
        "Docker にアクセスできません。Docker の権限または sudo の設定を確認してください。"
    )


def shell_command() -> str:
    """
    Docker コンテナ内で実行するシェルコマンドを返す。依存導入とビルドを行う。

    :return: 実行するシェルコマンドの文字列
    """

    build_script_args = [
        "python3",
        "tools/build.py",
        "--build-dir",
        BUILD_DIR,
        "--sdk-tag",
        SDK_TAG,
        "--build-type",
        BUILD_TYPE,
        "--target",
        TARGET,
        "--clean",
    ]
    build_command = " ".join(build_script_args)

    return (
        "apt-get update && "
        "apt-get install -y --no-install-recommends python3 && "
        f"{build_command}"
    )


def run_build_test() -> None:
    """
    Docker コンテナ内でビルドテストを実行する。依存導入とビルドを行う。

    :raises SystemExit: Docker コマンドの実行に失敗した場合
    """

    docker = docker_command_prefix()
    workspace = repo_root()
    mount_source = str(workspace.resolve())

    command = docker + [
        "run",
        "--rm",
        "-v",
        f"{mount_source}:/workspace",
        "-w",
        "/workspace",
        IMAGE,
        "bash",
        "-lc",
        shell_command(),
    ]

    print(f"+ {' '.join(command)}")
    subprocess.run(command, check=True)


def main() -> int:
    """
    メイン関数。

    :return: 0
    :raises SystemExit: Linux 環境でない場合
    """

    if platform.system() != "Linux":
        raise SystemExit("このスクリプトはLinux 環境で実行してください。")

    run_build_test()
    return 0


if __name__ == "__main__":
    sys.exit(main())
