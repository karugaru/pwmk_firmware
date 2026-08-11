from __future__ import annotations

import platform
import subprocess
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
TEST_SOURCE_DIR = REPOSITORY_ROOT / "test"
TEST_BUILD_DIR = TEST_SOURCE_DIR / "build"


class CommandFailure(Exception):
    def __init__(self, command: list[str], returncode: int) -> None:
        self.command = command
        self.returncode = returncode
        super().__init__(" ".join(command))


class TestFailure(Exception):
    def __init__(self, command: list[str], returncode: int) -> None:
        self.command = command
        self.returncode = returncode
        super().__init__(" ".join(command))


def execute_command(command: list[str]) -> subprocess.CompletedProcess[bytes]:
    print("+", " ".join(command), flush=True)
    try:
        return subprocess.run(command, cwd=REPOSITORY_ROOT, check=False)
    except OSError as error:
        raise CommandFailure(command, 127) from error


def run_command(command: list[str]) -> None:
    result = execute_command(command)
    if result.returncode != 0:
        raise CommandFailure(command, result.returncode)


def run_test(command: list[str]) -> None:
    result = execute_command(command)
    if result.returncode != 0:
        raise TestFailure(command, result.returncode)


def test_commands() -> tuple[list[str], ...]:
    return (
        ["cmake", "-S", str(TEST_SOURCE_DIR), "-B", str(TEST_BUILD_DIR)],
        ["cmake", "--build", str(TEST_BUILD_DIR)],
        ["ctest", "--test-dir", str(TEST_BUILD_DIR), "--output-on-failure"],
    )


def main() -> None:
    if platform.system() != "Linux":
        raise SystemExit("このスクリプトはLinux 環境で実行してください。")

    commands = test_commands()
    try:
        for command in commands[:-1]:
            run_command(command)
        run_test(commands[-1])
    except TestFailure as error:
        print(f"テストに失敗しました: {error}", file=sys.stderr)
        raise SystemExit(error.returncode) from None
    except CommandFailure as error:
        print(f"コマンドの実行に失敗しました: {error}", file=sys.stderr)
        raise SystemExit(error.returncode) from None


if __name__ == "__main__":
    main()
