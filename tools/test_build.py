from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import platform
import shutil
import subprocess
from typing import Annotated, TextIO

import typer

APT_CACHER_NG_IMAGE = "sameersbn/apt-cacher-ng:latest"
SDK_TAG = "2.3.0"
BUILD_DIR = "/tmp/pwmk-build-test"
BUILD_TYPE = "Release"
TARGET = "pwmk"
APT_CACHER_NG_CONTAINER = "pwmk-apt-cacher-ng"
APT_CACHER_NG_NETWORK = "pwmk-build-test-network"
APT_CACHER_NG_CACHE_VOLUME = "pwmk-apt-cacher-ng-cache"
PWMK_CACHE_VOLUME_PREFIX = "pwmk-pico-sdk-cache"
APT_PROXY_URL = f"http://{APT_CACHER_NG_CONTAINER}:3142"
LOG_DIR = "tools/log"
CONTAINER_PROJECT_ENVIRONMENT = "/tmp/pwmk-build-test-venv"
UV_INSTALL_COMMAND = (
    "curl -LsSf https://astral.sh/uv/install.sh | "
    'env UV_UNMANAGED_INSTALL="/usr/local/bin" sh'
)


@dataclass(frozen=True)
class BuildTestTarget:
    """
    ビルドテスト対象のディストリビューション定義。

    :param name: 表示用の対象名
    :param image: 利用する Docker イメージ
    :param bootstrap_command: コンテナ内で uv を用意するための初期化コマンド
    :param use_apt_proxy: apt-cacher-ng を利用するかどうか
    """

    name: str
    image: str
    bootstrap_command: str
    use_apt_proxy: bool = False


APT_DIRECT_BOOTSTRAP_COMMAND = (
    "apt-get update && "
    "apt-get install -y --no-install-recommends ca-certificates curl && "
    f"{UV_INSTALL_COMMAND}"
)

APT_PROXY_BOOTSTRAP_COMMAND = (
    "printf 'Acquire::http::Proxy \""
    + APT_PROXY_URL
    + "\";\\n' > /etc/apt/apt.conf.d/01proxy && "
    + APT_DIRECT_BOOTSTRAP_COMMAND
)

BUILD_TEST_TARGETS = [
    BuildTestTarget(
        name="ubuntu_24_04",
        image="ubuntu:24.04",
        bootstrap_command=APT_PROXY_BOOTSTRAP_COMMAND,
        use_apt_proxy=True,
    ),
    BuildTestTarget(
        name="ubuntu_26_04",
        image="ubuntu:26.04",
        bootstrap_command=APT_PROXY_BOOTSTRAP_COMMAND,
        use_apt_proxy=True,
    ),
    BuildTestTarget(
        name="debian_11_11",
        image="debian:11.11",
        bootstrap_command=APT_PROXY_BOOTSTRAP_COMMAND,
        use_apt_proxy=True,
    ),
    BuildTestTarget(
        name="debian_12_15",
        image="debian:12.15",
        bootstrap_command=APT_PROXY_BOOTSTRAP_COMMAND,
        use_apt_proxy=True,
    ),
    BuildTestTarget(
        name="alma_9_8",
        image="almalinux:9.8",
        bootstrap_command=(
            "dnf install -y ca-certificates curl-minimal && " f"{UV_INSTALL_COMMAND}"
        ),
    ),
    BuildTestTarget(
        name="alma_10_2",
        image="almalinux:10.2",
        bootstrap_command=(
            "dnf install -y ca-certificates curl && " f"{UV_INSTALL_COMMAND}"
        ),
    ),
    BuildTestTarget(
        name="fedora_43",
        image="fedora:43",
        bootstrap_command=(
            "dnf install -y ca-certificates curl && " f"{UV_INSTALL_COMMAND}"
        ),
    ),
    BuildTestTarget(
        name="fedora_44",
        image="fedora:44",
        bootstrap_command=(
            "dnf install -y ca-certificates curl && " f"{UV_INSTALL_COMMAND}"
        ),
    ),
    BuildTestTarget(
        name="arch_latest",
        image="archlinux:latest",
        bootstrap_command=(
            "pacman -Sy --noconfirm ca-certificates curl && " f"{UV_INSTALL_COMMAND}"
        ),
    ),
]

BUILD_TEST_TARGETS_BY_NAME = {target.name: target for target in BUILD_TEST_TARGETS}

app = typer.Typer(
    help="Docker 上で指定ディストリビューションのビルドテストを実行する。",
    add_completion=False,
)


def validate_targets(targets: list[str] | None) -> list[str] | None:
    """
    指定された対象名がサポート対象かどうかを検証する。

    :param targets: 検証対象の名前一覧
    :return: 検証済みの名前一覧
    """

    if not targets:
        return targets

    invalid_targets = [target for target in targets if target not in BUILD_TEST_TARGETS_BY_NAME]
    if invalid_targets:
        valid_targets = ", ".join(sorted(BUILD_TEST_TARGETS_BY_NAME))
        invalid = ", ".join(invalid_targets)
        raise typer.BadParameter(
            f"未対応の対象です: {invalid}. 指定可能: {valid_targets}"
        )

    return targets


def selected_targets(target_names: list[str] | None) -> list[BuildTestTarget]:
    """
    コマンドライン引数に応じたテスト対象一覧を返す。

    :param target_names: 対象ディストリビューション名一覧
    :return: 実行対象ディストリビューション一覧
    """

    if not target_names:
        return BUILD_TEST_TARGETS

    return [BUILD_TEST_TARGETS_BY_NAME[target_name] for target_name in target_names]


def repo_root() -> Path:
    """
    リポジトリのルートディレクトリを返す。

    :return: リポジトリのルートディレクトリの Path オブジェクト
    """

    return Path(__file__).resolve().parents[1]


def logs_root() -> Path:
    """
    テストログの出力先ディレクトリを返す。

    :return: テストログの出力先ディレクトリ
    """

    return repo_root() / LOG_DIR


def ensure_logs_directory() -> Path:
    """
    テストログの出力先ディレクトリを作成して返す。

    :return: 利用可能なログディレクトリ
    """

    directory = logs_root()
    directory.mkdir(parents=True, exist_ok=True)
    return directory


def target_log_path(target: BuildTestTarget) -> Path:
    """
    対象ディストリビューション用のログファイルパスを返す。

    :param target: ビルドテスト対象のディストリビューション定義
    :return: ログファイルパス
    """

    return ensure_logs_directory() / f"test_build_{target.name}.log"


def pwmk_cache_volume(target: BuildTestTarget) -> str:
    """
    対象ディストリビューション専用の PWMK キャッシュボリューム名を返す。

    :param target: ビルドテスト対象のディストリビューション定義
    :return: Docker ボリューム名
    """

    return f"{PWMK_CACHE_VOLUME_PREFIX}-{target.name}"


def setup_log_path() -> Path:
    """
    テスト準備処理用のログファイルパスを返す。

    :return: ログファイルパス
    """

    return ensure_logs_directory() / "test_build_setup.log"


def summary_log_path() -> Path:
    """
    テスト結果サマリ用のログファイルパスを返す。

    :return: ログファイルパス
    """

    return ensure_logs_directory() / "test_build_summary.log"


def write_log_header(log_handle: TextIO, command: list[str]) -> None:
    """
    ログ先頭に実行コマンドを書き込む。

    :param log_handle: 書き込み先ログハンドル
    :param command: 実行コマンド
    """

    log_handle.write(f"$ {' '.join(command)}\n\n")
    log_handle.flush()


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

    with setup_log_path().open("a", encoding="utf-8") as log_handle:
        command = docker + ["network", "create", network_name]
        write_log_header(log_handle, command)
        subprocess.run(
            command,
            check=True,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            text=True,
        )


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

        with setup_log_path().open("a", encoding="utf-8") as log_handle:
            command = docker + ["start", APT_CACHER_NG_CONTAINER]
            write_log_header(log_handle, command)
            subprocess.run(
                command,
                check=True,
                stdout=log_handle,
                stderr=subprocess.STDOUT,
                text=True,
            )
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

    with setup_log_path().open("a", encoding="utf-8") as log_handle:
        write_log_header(log_handle, command)
        subprocess.run(
            command,
            check=True,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            text=True,
        )


def shell_command(target: BuildTestTarget) -> str:
    """
    Docker コンテナ内で実行するシェルコマンドを返す。uv 導入後に依存同期とビルドを行う。

    :param target: ビルドテスト対象のディストリビューション定義
    :return: 実行するシェルコマンドの文字列
    """

    project_environment_prefix = (
        f'env UV_PROJECT_ENVIRONMENT="{CONTAINER_PROJECT_ENVIRONMENT}"'
    )

    build_script_args = [
        project_environment_prefix,
        "uv",
        "run",
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

    sync_command = f"{project_environment_prefix} uv sync --frozen"
    return f"{target.bootstrap_command} && {sync_command} && {build_command}"


def run_build_test_target(docker: list[str], target: BuildTestTarget) -> int:
    """
    単一ディストリビューション向けに Docker コンテナ内でビルドテストを実行する。

    :param docker: Docker コマンドのプレフィックス
    :param target: ビルドテスト対象のディストリビューション定義
    :return: コンテナ実行の終了コード
    """

    workspace = repo_root()
    mount_source = str(workspace.resolve())

    command = docker + [
        "run",
        "--rm",
        "-v",
        f"{mount_source}:/workspace",
        "-v",
        f"{pwmk_cache_volume(target)}:/root/.pwmk",
        "-w",
        "/workspace",
    ]

    if target.use_apt_proxy:
        command.extend(
            [
                "--network",
                APT_CACHER_NG_NETWORK,
            ]
        )

    command.extend(
        [
            target.image,
            "bash",
            "-lc",
            shell_command(target),
        ]
    )

    log_path = target_log_path(target)
    print(f"[{target.name}] 実行中... ログ: {log_path}")
    with log_path.open("w", encoding="utf-8") as log_handle:
        write_log_header(log_handle, command)
        result = subprocess.run(
            command,
            check=False,
            stdout=log_handle,
            stderr=subprocess.STDOUT,
            text=True,
        )
        log_handle.write(f"\nexit code: {result.returncode}\n")

    print(f"[{target.name}] 完了 exit code={result.returncode}")
    return result.returncode


def run_build_tests(targets: list[BuildTestTarget], no_proxy: bool) -> int:
    """
    指定された対象ディストリビューションで Docker ビルドテストを実行し、結果を集計する。

    :param targets: 実行対象ディストリビューション一覧
    :param no_proxy: APT プロキシを使用せずにブートストラップを実行するかどうか
    :return: すべて成功した場合は 0、それ以外は 1
    """

    docker = docker_command_prefix()
    ensure_logs_directory()
    if any(target.use_apt_proxy for target in targets) and not no_proxy:
        ensure_apt_cacher_ng(docker)

    failed_targets: list[str] = []
    summary_lines: list[str] = []
    for target in targets:
        if target.use_apt_proxy and no_proxy:
            target = BuildTestTarget(
                name=target.name,
                image=target.image,
                bootstrap_command=APT_DIRECT_BOOTSTRAP_COMMAND,
                use_apt_proxy=False,
            )
        exit_code = run_build_test_target(docker, target)
        summary_lines.append(f"{target.name}: exit code {exit_code}")
        if exit_code != 0:
            failed_targets.append(f"{target.name}({exit_code})")

    with summary_log_path().open("w", encoding="utf-8") as log_handle:
        log_handle.write("\n".join(summary_lines) + "\n")

    print("\n=== build test summary ===")
    if not failed_targets:
        print("all targets succeeded")
        print(f"summary log: {summary_log_path()}")
        return 0

    print("failed targets: " + ", ".join(failed_targets))
    print(f"summary log: {summary_log_path()}")
    return 1


@app.command()
def run_command(
    targets: Annotated[
        list[str] | None,
        typer.Argument(
            help="テスト対象ディストリビューション名。省略時は全件実行する。",
            callback=validate_targets,
        ),
    ] = None,
    use_proxy: Annotated[
        bool,
        typer.Option(
            "--use-proxy/--no-proxy",
            help="APT プロキシを使用してブートストラップを実行する。",
        ),
    ] = True,
) -> int:
    """
    ビルドテスト CLI コマンド。
    """

    if platform.system() != "Linux":
        raise SystemExit("このスクリプトはLinux 環境で実行してください。")

    return run_build_tests(selected_targets(targets), not use_proxy)


if __name__ == "__main__":
    app(prog_name="test_build")
