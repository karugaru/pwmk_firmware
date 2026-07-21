from __future__ import annotations

import argparse
from collections.abc import Sequence

from pwmk_build import add_build_subcommand


def create_parser() -> argparse.ArgumentParser:
    """
    PWMK CLI の引数パーサーを構築する。

    :return: 引数パーサー
    """

    parser = argparse.ArgumentParser(prog="pwmk", description="PWMK 用 CLI。")
    subparsers = parser.add_subparsers(dest="subcommand")
    add_build_subcommand(subparsers)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """
    PWMK CLI のエントリポイント。

    :param argv: 解析対象の引数列
    :return: 終了ステータスコード
    """

    parser = create_parser()
    args = parser.parse_args(list(argv) if argv is not None else None)
    handler = getattr(args, "handler", None)
    if handler is None:
        parser.print_help()
        return 1

    return handler(args)
