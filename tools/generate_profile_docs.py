from __future__ import annotations

import argparse
import json
import unicodedata
from pathlib import Path
from typing import Any

from pwmk_common import repo_root
from pwmk_profile_schema import ProfileConfig

ROOT_TITLE = "プロファイル設定"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="ProfileConfig の JSON Schema から Markdown ドキュメントを生成する。"
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=repo_root() / "docs" / f"{ROOT_TITLE}.md",
        help="出力先 Markdown ファイルパス。",
    )
    return parser.parse_args()


def build_markdown(schema: dict[str, Any]) -> str:
    definitions = schema.get("$defs", {})
    lines = [
        f"# {ROOT_TITLE}",
        "",
        "このドキュメントは `uv run tools/generate_profile_docs.py` により自動生成されます。",
        "`keyboard-layout.json` の内容は KLE互換の(非raw)JSON形式で記述します。",
        "",
        "## トップレベル",
        "",
    ]
    lines.extend(render_object_table(schema, definitions))

    top_level_properties = schema.get("properties", {})
    for field_name, field_schema in top_level_properties.items():
        target_schema = dereference(field_schema, definitions)
        if not is_object_schema(target_schema):
            continue
        lines.extend(render_section(field_name, field_schema, definitions, level=2))

    lines.extend(
        [
            "",
            "## 相互検証",
            "",
            "生成時には、個別項目の型・範囲制約に加えて、次の組み合わせも検証します。",
            "",
            "- `board.layout` の要素数が `rows_pins` と `cols_pins` の容量を超えないこと。",
            "- `board.layout` の座標が範囲内で重複していないこと。",
            "- `board.vial_unlock_combo` を指定した場合、全座標がレイアウト上にあり重複していないこと。",
            "- `keymap.keymap` の要素数が `board.layout` の要素数と一致すること。",
            "- `settings.use_pinnacle` が `true` の場合、`settings.pinnacle` が指定されていること。",
        ]
    )

    return "\n".join(lines) + "\n"


def render_section(
    section_name: str,
    schema: dict[str, Any],
    definitions: dict[str, Any],
    *,
    level: int,
) -> list[str]:
    resolved = dereference(schema, definitions)
    lines = ["", f"{'#' * level} {section_name}", ""]

    description = schema.get("description") or resolved.get("description")
    if description:
        lines.extend([description, ""])

    lines.extend(render_object_table(resolved, definitions))

    for child_name, child_schema in resolved.get("properties", {}).items():
        child_resolved = dereference(child_schema, definitions)
        if is_object_schema(child_resolved):
            child_section_name = f"{section_name}.{child_name}"
            lines.extend(
                render_section(
                    child_section_name, child_schema, definitions, level=level + 1
                )
            )

    return lines


def render_object_table(
    schema: dict[str, Any],
    definitions: dict[str, Any],
) -> list[str]:
    required_fields = set(schema.get("required", []))
    rows = [["項目", "型", "必須", "既定値", "説明", "制約"]]

    for field_name, field_schema in schema.get("properties", {}).items():
        resolved = dereference(field_schema, definitions)
        field_type = format_type(field_schema, definitions)
        required = "はい" if field_name in required_fields else "いいえ"
        default = format_default(field_schema, resolved)
        description = escape_table_text(
            field_schema.get("description") or resolved.get("description") or ""
        )
        constraints = escape_table_text(format_constraints(field_schema, resolved))
        rows.append(
            [
                escape_table_text(field_name),
                escape_table_text(field_type),
                required,
                escape_table_text(default),
                description,
                constraints,
            ]
        )

    widths = [
        max(markdown_display_width(row[index]) for row in rows)
        for index in range(len(rows[0]))
    ]
    lines = [format_markdown_table_row(rows[0], widths)]
    lines.append(format_markdown_table_row(["-" * width for width in widths], widths))
    lines.extend(format_markdown_table_row(row, widths) for row in rows[1:])

    return lines


def markdown_display_width(text: str) -> int:
    """Markdown 表の列揃えに使用する文字列の表示幅を返す。"""
    return sum(
        2 if unicodedata.east_asian_width(character) in ("F", "W") else 1
        for character in text
    )


def format_markdown_table_row(cells: list[str], widths: list[int]) -> str:
    """指定した列幅で Markdown 表の 1 行を整形する。"""
    padded_cells = [
        cell + " " * (width - markdown_display_width(cell))
        for cell, width in zip(cells, widths, strict=True)
    ]
    return "| " + " | ".join(padded_cells) + " |"


def dereference(schema: dict[str, Any], definitions: dict[str, Any]) -> dict[str, Any]:
    reference = schema.get("$ref")
    if reference:
        definition_name = reference.removeprefix("#/$defs/")
        return definitions[definition_name]

    any_of = schema.get("anyOf")
    if any_of:
        non_null_schemas = [
            option
            for option in any_of
            if not (option.get("type") == "null" and len(option) == 1)
        ]
        if len(non_null_schemas) == 1:
            return dereference(non_null_schemas[0], definitions)

    return schema


def is_object_schema(schema: dict[str, Any]) -> bool:
    return schema.get("type") == "object" and "properties" in schema


def format_type(schema: dict[str, Any], definitions: dict[str, Any]) -> str:
    if "$ref" in schema:
        return schema["$ref"].removeprefix("#/$defs/")

    any_of = schema.get("anyOf")
    if any_of:
        nullable = False
        option_types: list[str] = []
        for option in any_of:
            if option.get("type") == "null" and len(option) == 1:
                nullable = True
                continue
            option_types.append(format_type(option, definitions))
        if nullable:
            option_types.append("null")
        return " | ".join(option_types)

    schema_type = schema.get("type")
    if schema_type == "array":
        prefix_items = schema.get("prefixItems")
        if prefix_items:
            item_types = ", ".join(
                format_type(item_schema, definitions) for item_schema in prefix_items
            )
            return f"tuple[{item_types}]"
        items = schema.get("items")
        if isinstance(items, dict):
            return f"list[{format_type(items, definitions)}]"
        return "list"
    if schema_type == "object":
        additional = schema.get("additionalProperties")
        if isinstance(additional, dict):
            return f"dict[str, {format_type(additional, definitions)}]"
        return "object"
    if schema_type is None:
        return "unknown"
    return schema_type


def format_default(schema: dict[str, Any], resolved: dict[str, Any]) -> str:
    if "default" in schema:
        return json.dumps(schema["default"], ensure_ascii=False)
    if "default" in resolved:
        return json.dumps(resolved["default"], ensure_ascii=False)
    return "-"


def format_constraints(schema: dict[str, Any], resolved: dict[str, Any]) -> str:
    source = merge_constraint_sources(schema, resolved)
    constraints: list[str] = []

    if "minLength" in source:
        constraints.append(f"最小文字数 {source['minLength']}")
    if "maxLength" in source:
        constraints.append(f"最大文字数 {source['maxLength']}")
    if "minimum" in source:
        constraints.append(f"最小値 {source['minimum']}")
    if "maximum" in source:
        constraints.append(f"最大値 {source['maximum']}")
    if "minItems" in source:
        constraints.append(f"最小要素数 {source['minItems']}")
    if "maxItems" in source:
        constraints.append(f"最大要素数 {source['maxItems']}")

    any_of = source.get("anyOf")
    if any_of:
        option_names = []
        for option in any_of:
            if option.get("type") == "null" and len(option) == 1:
                option_names.append("null 許容")
                continue
            option_names.append(format_type(option, {}))
        if option_names:
            constraints.append("候補: " + ", ".join(option_names))

    return ", ".join(constraints) if constraints else "-"


def merge_constraint_sources(
    schema: dict[str, Any], resolved: dict[str, Any]
) -> dict[str, Any]:
    merged = dict(resolved)
    merged.update(schema)
    return merged


def escape_table_text(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", "<br>")


def main() -> None:
    args = parse_args()
    schema = ProfileConfig.model_json_schema()
    markdown = build_markdown(schema)
    args.output.write_text(markdown, encoding="utf-8")
    print(args.output.relative_to(repo_root()).as_posix())


if __name__ == "__main__":
    main()
