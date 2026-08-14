from __future__ import annotations

import re
import subprocess
from pathlib import Path
from typing import Annotated

import typer

EMBEDDED_DRIVE_PATTERN = re.compile(
    r"^\s*embedded drive:\s+"
    r"0x(?P<start>[0-9a-f]+)-0x(?P<end>[0-9a-f]+)\s+"
    r"\([^)]*\):\s+(?P<name>.*?)\s+flags\s+0x[0-9a-f]+"
    r"(?:\s+[rwx]+)?\s*$",
    re.IGNORECASE | re.MULTILINE,
)


def register_verify_binary_command(app: typer.Typer) -> None:
    """
    verify-binary サブコマンドを登録する。

    :param app: サブコマンド登録先
    """

    @app.command("verify-binary", help="ファームウェアの配置範囲を検証する。")
    def verify_binary_command(
        picotool: Annotated[
            Path,
            typer.Option(help="実行するpicotoolのパス。"),
        ],
        binary: Annotated[
            Path,
            typer.Option(help="検証対象のファームウェアバイナリ。"),
        ],
        name: Annotated[
            str,
            typer.Option(help="picotoolで検索する埋め込みドライブ名。"),
        ],
    ) -> None:
        verify_binary(picotool, binary, name)


def read_firmware_limit(output: str, name: str) -> int:
    matches = [
        match
        for match in EMBEDDED_DRIVE_PATTERN.finditer(output)
        if match.group("name") == name
    ]
    if len(matches) != 1:
        raise ValueError(f"embedded firmware area was not found uniquely: {name}")

    match = matches[0]
    start = int(match.group("start"), 16)
    end = int(match.group("end"), 16)
    if start != 0 or end <= start:
        raise ValueError(f"invalid embedded firmware area: 0x{start:x}-0x{end:x}")
    return end


def verify_binary(picotool: Path, binary: Path, name: str) -> None:
    """
    ファームウェアが永続化領域へ重ならないことを検証する。

    :param picotool: 実行するpicotoolのパス
    :param binary: 検証対象のファームウェアバイナリ
    :param name: picotoolで検索する埋め込みドライブ名
    """

    result = subprocess.run(
        [str(picotool), "info", "-a", str(binary)],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != 0:
        raise SystemExit(
            f"picotool info failed with exit code {result.returncode}:\n"
            f"{result.stderr.strip()}"
        )

    try:
        firmware_limit = read_firmware_limit(result.stdout, name)
    except ValueError as error:
        raise SystemExit(str(error)) from error

    firmware_size = binary.stat().st_size
    if firmware_size > firmware_limit:
        raise SystemExit(
            f"firmware overlaps the persistence area: "
            f"{firmware_size} bytes > {firmware_limit} bytes"
        )

    print(
        f"firmware size: {firmware_size} bytes, " f"available: {firmware_limit} bytes"
    )
