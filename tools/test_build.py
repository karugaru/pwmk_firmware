from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import platform
import shutil
from typing import Annotated, TextIO

from python_on_whales import DockerClient
from python_on_whales.exceptions import DockerException
import typer

APT_CACHER_NG_IMAGE = "sameersbn/apt-cacher-ng:latest"
SDK_TAG = "2.3.0"
BUILD_DIR = "/tmp/pwmk-build-test"
BUILD_TYPE = "Release"
TARGET = "pwmk"
PROFILE = "remopicon_v1"
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


@dataclass(frozen=True)
class ResolvedDockerClient:
    """
    実行可能な DockerClient と、その起動コマンド列を保持する。

    :param client: python-on-whales の DockerClient
    """

    client: DockerClient


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

    invalid_targets = [
        target for target in targets if target not in BUILD_TEST_TARGETS_BY_NAME
    ]
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


def ensure_command(name: str) -> None:
    """
    指定されたコマンドが存在することを確認する。存在しない場合は SystemExit を発生させる。

    :param name: 確認するコマンドの名前
    :raises SystemExit: コマンドが存在しない場合
    """

    if shutil.which(name) is None:
        raise SystemExit(f"次の必要なコマンドが見つかりません: {name}")


def append_setup_error_log(context: str, error: DockerException) -> None:
    """
    Docker セットアップ処理の失敗内容だけをログへ追記する。

    :param context: どの処理で失敗したかを表す文字列
    :param error: 送出された Docker 例外
    """

    with setup_log_path().open("a", encoding="utf-8") as log_handle:
        log_handle.write(f"context: {context}\n")
        log_docker_exception(log_handle, error)
        log_handle.write("\n")


def log_docker_exception(log_handle: TextIO, error: DockerException) -> None:
    """
    python-on-whales が返した Docker 実行エラー詳細をログへ書き込む。

    :param log_handle: 書き込み先ログハンドル
    :param error: 送出された Docker 例外
    """

    log_handle.write(f"docker command: {' '.join(error.docker_command)}\n")
    log_handle.write(f"return code: {error.return_code}\n")
    if error.stdout:
        log_handle.write("\n[stdout]\n")
        log_handle.write(error.stdout)
        if not error.stdout.endswith("\n"):
            log_handle.write("\n")
    if error.stderr:
        log_handle.write("\n[stderr]\n")
        log_handle.write(error.stderr)
        if not error.stderr.endswith("\n"):
            log_handle.write("\n")
    log_handle.flush()


def docker_client() -> ResolvedDockerClient:
    """
    利用可能な Docker クライアントを返す。sudo が必要な環境では sudo 経由の client_call を使用する。

    :return: 利用可能な DockerClient
    """

    ensure_command("docker")
    direct_call = ["docker"]
    direct = DockerClient(client_call=direct_call, client_type="docker")
    try:
        direct.version()
        return ResolvedDockerClient(client=direct)
    except DockerException:
        pass

    if shutil.which("sudo") is not None:
        sudo_call = ["sudo", "-n", "docker"]
        sudo_direct = DockerClient(
            client_call=sudo_call,
            client_type="docker",
        )
        try:
            sudo_direct.version()
            return ResolvedDockerClient(client=sudo_direct)
        except DockerException:
            pass

    raise SystemExit(
        "Docker にアクセスできません。Docker の権限または sudo の設定を確認してください。"
    )


def ensure_network(docker: ResolvedDockerClient, network_name: str) -> None:
    """
    指定された Docker ネットワークが存在することを確認し、存在しない場合は作成する。

    :param docker: Docker コマンドのプレフィックス
    :param network_name: 確認または作成するネットワーク名
    """

    if docker.client.network.exists(network_name):
        return

    try:
        docker.client.network.create(network_name)
    except DockerException as error:
        append_setup_error_log(f"docker.network.create network={network_name}", error)
        raise


def ensure_apt_cacher_ng(docker: ResolvedDockerClient) -> None:
    """
    apt-cacher-ng コンテナが利用可能であることを確認する。存在しない場合は作成し、停止中なら起動する。

    :param docker: Docker コマンドのプレフィックス
    """

    ensure_network(docker, APT_CACHER_NG_NETWORK)

    if docker.client.container.exists(APT_CACHER_NG_CONTAINER):
        container = docker.client.container.inspect(APT_CACHER_NG_CONTAINER)
        if container.state.running:
            return

        try:
            docker.client.start(container)
        except DockerException as error:
            append_setup_error_log(
                f"docker.container.start container={APT_CACHER_NG_CONTAINER}",
                error,
            )
            raise
        return

    try:
        container = docker.client.create(
            APT_CACHER_NG_IMAGE,
            name=APT_CACHER_NG_CONTAINER,
            restart="unless-stopped",
            volumes=[(APT_CACHER_NG_CACHE_VOLUME, "/var/cache/apt-cacher-ng")],
        )
        docker.client.network.connect(APT_CACHER_NG_NETWORK, container)
        docker.client.start(container)
    except DockerException as error:
        append_setup_error_log(
            (
                "docker.container.create "
                f"container={APT_CACHER_NG_CONTAINER} image={APT_CACHER_NG_IMAGE}"
            ),
            error,
        )
        raise


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
        "--profile",
        PROFILE,
        "--build-dir",
        BUILD_DIR,
        "--sdk-tag",
        SDK_TAG,
        "--build-type",
        BUILD_TYPE,
        "--target",
        TARGET,
    ]
    build_command = " ".join(build_script_args)

    sync_command = f"{project_environment_prefix} uv sync --frozen"
    return f"{target.bootstrap_command} && {sync_command} && {build_command}"


def run_build_test_target(docker: ResolvedDockerClient, target: BuildTestTarget) -> int:
    """
    単一ディストリビューション向けに Docker コンテナ内でビルドテストを実行する。

    :param docker: Docker コマンドのプレフィックス
    :param target: ビルドテスト対象のディストリビューション定義
    :return: コンテナ実行の終了コード
    """

    workspace = repo_root()
    mount_source = str(workspace.resolve())

    log_path = target_log_path(target)
    print(f"[{target.name}] 実行中... ログ: {log_path}")
    container = None
    with log_path.open("w", encoding="utf-8") as log_handle:
        try:
            container = docker.client.create(
                target.image,
                ["bash", "-lc", shell_command(target)],
                volumes=[
                    (mount_source, "/workspace"),
                    (pwmk_cache_volume(target), "/root/.pwmk"),
                ],
                workdir="/workspace",
            )
            if target.use_apt_proxy:
                docker.client.network.connect(APT_CACHER_NG_NETWORK, container)
            docker.client.start(container)
            for _, content in docker.client.logs(container, follow=True, stream=True):
                if isinstance(content, bytes):
                    log_handle.write(content.decode("utf-8", errors="replace"))
                else:
                    log_handle.write(str(content))
            exit_code = docker.client.wait(container)
        except DockerException as error:
            log_docker_exception(log_handle, error)
            exit_code = error.return_code or 1
        finally:
            if container is not None and docker.client.container.exists(container):
                docker.client.remove(container, force=True)

        log_handle.write(f"\nexit code: {exit_code}\n")

    print(f"[{target.name}] 完了 exit code={exit_code}")
    return exit_code


def run_build_tests(targets: list[BuildTestTarget], no_proxy: bool) -> int:
    """
    指定された対象ディストリビューションで Docker ビルドテストを実行し、結果を集計する。

    :param targets: 実行対象ディストリビューション一覧
    :param no_proxy: APT プロキシを使用せずにブートストラップを実行するかどうか
    :return: すべて成功した場合は 0、それ以外は 1
    """

    docker = docker_client()
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
