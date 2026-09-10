from __future__ import annotations

import argparse
import json
from pathlib import Path

from pwmk_common import repo_root
from pwmk_profile_schema import ProfileConfig

DEFAULT_OUTPUT = repo_root() / ".vscode" / "profile.schema.json"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="ProfileConfig の JSON Schema を生成する。"
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="出力先 JSON ファイルパス。",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    schema = ProfileConfig.model_json_schema()
    schema["$schema"] = "https://json-schema.org/draft/2020-12/schema"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(schema, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
