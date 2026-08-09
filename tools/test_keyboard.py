from __future__ import annotations

import platform
import subprocess
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
TEST_SOURCE_DIR = REPOSITORY_ROOT / "test"
TEST_BUILD_DIR = TEST_SOURCE_DIR / "build"


def run_command(command: list[str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=REPOSITORY_ROOT, check=True)


def test_commands() -> tuple[list[str], ...]:
    return (
        ["cmake", "-S", str(TEST_SOURCE_DIR), "-B", str(TEST_BUILD_DIR)],
        ["cmake", "--build", str(TEST_BUILD_DIR)],
        ["ctest", "--test-dir", str(TEST_BUILD_DIR), "--output-on-failure"],
    )


def main() -> None:
    if platform.system() != "Linux":
        raise SystemExit("このスクリプトはLinux 環境で実行してください。")

    for command in test_commands():
        run_command(command)


if __name__ == "__main__":
    main()
