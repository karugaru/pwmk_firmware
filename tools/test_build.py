from __future__ import annotations

import platform
import shutil
import subprocess
import sys
from pathlib import Path

IMAGE = "ubuntu:24.04"
APT_CACHER_NG_IMAGE = "sameersbn/apt-cacher-ng:latest"
SDK_TAG = "2.3.0"
BUILD_DIR = "/tmp/pwmk-build-test"
BUILD_TYPE = "Release"
TARGET = "pwmk"
APT_CACHER_NG_CONTAINER = "pwmk-apt-cacher-ng"
APT_CACHER_NG_NETWORK = "pwmk-build-test-network"
APT_CACHER_NG_CACHE_VOLUME = "pwmk-apt-cacher-ng-cache"
PICO_SDK_CACHE_VOLUME = "pwmk-pico-sdk-cache"
APT_PROXY_URL = f"http://{APT_CACHER_NG_CONTAINER}:3142"


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


def ensure_network(docker: list[str], network_name: str) -> None:
    """
    指定された Docker ネットワークが存在することを確認し、存在しない場合は作成する。

    :param docker: Docker コマンドのプレフィックス
    :param network_name: 確認または作成するネットワーク名
    """

    if completed_process(docker + ["network", "inspect", network_name]).returncode == 0:
        return

    subprocess.run(docker + ["network", "create", network_name], check=True)


def ensure_apt_cacher_ng(docker: list[str]) -> None:
    """
    apt-cacher-ng コンテナが利用可能であることを確認する。存在しない場合は作成し、停止中なら起動する。

    :param docker: Docker コマンドのプレフィックス
    """

    ensure_network(docker, APT_CACHER_NG_NETWORK)

    inspect = completed_process(
        docker
        + [
            "container",
            "inspect",
            "-f",
            "{{.State.Running}}",
            APT_CACHER_NG_CONTAINER,
        ]
    )
    if inspect.returncode == 0:
        if inspect.stdout.strip() == "true":
            return

        subprocess.run(docker + ["start", APT_CACHER_NG_CONTAINER], check=True)
        return

    command = docker + [
        "run",
        "-d",
        "--restart",
        "unless-stopped",
        "--name",
        APT_CACHER_NG_CONTAINER,
        "--network",
        APT_CACHER_NG_NETWORK,
        "-v",
        f"{APT_CACHER_NG_CACHE_VOLUME}:/var/cache/apt-cacher-ng",
        APT_CACHER_NG_IMAGE,
    ]

    print(f"+ {' '.join(command)}")
    subprocess.run(command, check=True)


def shell_command() -> str:
    """
    Docker コンテナ内で実行するシェルコマンドを返す。依存導入とビルドを行う。

    :return: 実行するシェルコマンドの文字列
    """

    build_script_args = [
        "python3",
        "tools/pwmk.py",
        "build",
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
        "printf 'Acquire::http::Proxy \""
        + APT_PROXY_URL
        + "\";\\n' > /etc/apt/apt.conf.d/01proxy && "
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
    ensure_apt_cacher_ng(docker)

    workspace = repo_root()
    mount_source = str(workspace.resolve())

    command = docker + [
        "run",
        "--rm",
        "--network",
        APT_CACHER_NG_NETWORK,
        "-v",
        f"{mount_source}:/workspace",
        "-v",
        f"{PICO_SDK_CACHE_VOLUME}:/root/.pwmk",
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
