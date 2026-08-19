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


def pwmk_cache_root() -> Path:
    """
    PWMK の外部依存キャッシュルートを返す。

    :return: 外部依存キャッシュルートの Path オブジェクト
    """

    return Path.home() / ".pwmk"


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
    print(f"\n+ {printable}", flush=True)
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
    print(f"\n+ {printable}", flush=True)
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


def safe_rmtree(
    path: Path, *, ignore_errors: bool = False, allow_pwmk_cache: bool = False
) -> None:
    """
    リポジトリ配下だけを削除する。必要な場合は ~/.pwmk 配下も許可する。

    allow_pwmk_cache が True の場合だけ、~/.pwmk 配下も削除できる。

    :param path: 削除対象ディレクトリ
    :param ignore_errors: shutil.rmtree に渡すエラー無視フラグ
    :param allow_pwmk_cache: ~/.pwmk 配下の削除を許可するかどうか
    :raises ValueError: 許可範囲外、許可ルート自身、またはシンボリックリンクが指定された場合
    """

    lexical_path = Path(path)
    if not lexical_path.is_absolute():
        lexical_path = Path.cwd() / lexical_path
    if lexical_path.is_symlink():
        raise ValueError(f"シンボリックリンクは削除できません: {path}")

    resolved_path = lexical_path.resolve()
    allowed_roots = [repo_root().resolve()]
    if allow_pwmk_cache:
        allowed_roots.append(pwmk_cache_root().resolve())

    if not any(
        resolved_path != allowed_root and allowed_root in resolved_path.parents
        for allowed_root in allowed_roots
    ):
        raise ValueError(f"許可された削除範囲外のディレクトリです: {resolved_path}")

    if resolved_path.exists():
        shutil.rmtree(resolved_path, ignore_errors=ignore_errors)
