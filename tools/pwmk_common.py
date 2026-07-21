from __future__ import annotations

import platform
import shutil
import subprocess
from pathlib import Path


def repo_root() -> Path:
    """
    リポジトリのルートディレクトリを返す。

    :return: リポジトリのルートディレクトリの Path オブジェクト
    """

    return Path(__file__).resolve().parents[1]


def ensure_linux() -> None:
    """
    実行環境が Linux であることを確認する。

    :raises SystemExit: Linux 環境でない場合
    """

    if platform.system() != "Linux":
        raise SystemExit("このスクリプトはLinux 環境で実行してください。")


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
