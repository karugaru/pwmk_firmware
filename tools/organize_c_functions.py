#!/usr/bin/env python3
"""
C関数を、宣言、パブリック定義、プライベート定義の順に並べ替えます。

このプロジェクトのソースファイルにはGCC拡張機能が含まれており、
C ASTプリンタを介したラウンドトリップには適していません。
そのため、このツールはソース範囲の解析にC言語対応のレクサーを使用し、
移動されたすべてのブロックを元のソーステキストのまま保持します。
また、ファイルスコープの関数の宣言と定義のみを移動します。
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Annotated

import typer

_FUNCTION_RE = re.compile(
    r"(?P<name>(?<![A-Za-z0-9_])(?!__attribute__\b|__declspec\b)"
    r"[A-Za-z_]\w*)\s*"
    r"\([^;{}]*\)\s*"
    r"(?:__attribute__\s*\(\([^;{}]*\)\)\s*)*$",
    re.DOTALL,
)
_CONDITIONAL_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b")
_GENERATED_SECTION_RE = re.compile(
    r"/\*\s*\n\s*\* (?:内部関数宣言|公開関数|内部関数)\s*\n\s*\*/"
)
_LEGACY_SECTION_LINE_RE = re.compile(
    r"(?m)^[ \t]*//(?:\s*[-=]+|\s*(?:関数宣言|関数定義|静的関数|公開関数|内部関数))\s*\r?\n"
)


@dataclass(frozen=True)
class FunctionUnit:
    start: int
    end: int
    kind: str
    name: str
    signature: str = ""
    generated_signatures: tuple[str, ...] = ()


@dataclass
class ConditionalRegion:
    start: int
    if_line_end: int
    branches: list[tuple[int, int, int | None, int | None]]
    endif_start: int
    endif_end: int


def _blank_comments_and_literals(source: str) -> str:
    """コメントとリテラルを空白に置き換え、行内の位置を維持する。"""
    chars = list(source)
    index = 0
    length = len(source)
    state = "normal"

    while index < length:
        current = source[index]
        next_char = source[index + 1] if index + 1 < length else ""

        if state == "normal":
            if current == "/" and next_char == "/":
                chars[index] = " "
                chars[index + 1] = " "
                index += 2
                state = "line_comment"
                continue
            if current == "/" and next_char == "*":
                chars[index] = " "
                chars[index + 1] = " "
                index += 2
                state = "block_comment"
                continue
            if current == '"':
                chars[index] = " "
                index += 1
                state = "string"
                continue
            if current == "'":
                chars[index] = " "
                index += 1
                state = "character"
                continue
            index += 1
            continue

        if state == "line_comment":
            if current == "\n":
                state = "normal"
            else:
                chars[index] = " "
            index += 1
            continue

        if state == "block_comment":
            if current == "*" and next_char == "/":
                chars[index] = " "
                chars[index + 1] = " "
                index += 2
                state = "normal"
            else:
                if current != "\n":
                    chars[index] = " "
                index += 1
            continue

        if state in ("string", "character"):
            if current == "\\" and next_char:
                chars[index] = " "
                if next_char != "\n":
                    chars[index + 1] = " "
                index += 2
            elif (state == "string" and current == '"') or (
                state == "character" and current == "'"
            ):
                chars[index] = " "
                index += 1
                state = "normal"
            else:
                if current != "\n":
                    chars[index] = " "
                index += 1

    return "".join(chars)


def _blank_preprocessor_lines(source: str, basic_mask: str) -> str:
    """継続行を含むプリプロセッサディレクティブを空白に置き換える。"""
    chars = list(basic_mask)
    in_directive = False
    offset = 0

    for line in source.splitlines(keepends=True):
        line_end = offset + len(line)
        masked_line = basic_mask[offset:line_end]
        starts_directive = bool(re.match(r"^[ \t]*#", masked_line))
        if starts_directive or in_directive:
            for index in range(offset, line_end):
                if source[index] != "\n":
                    chars[index] = " "
            in_directive = masked_line.rstrip("\r\n").endswith("\\")
        else:
            in_directive = False
        offset = line_end

    return "".join(chars)


def _line_offsets(source: str) -> list[tuple[int, int]]:
    offsets: list[tuple[int, int]] = []
    offset = 0
    for line in source.splitlines(keepends=True):
        offsets.append((offset, offset + len(line)))
        offset += len(line)
    if not offsets or offset < len(source):
        offsets.append((offset, len(source)))
    return offsets


def _top_level_conditional_regions(source: str) -> list[ConditionalRegion]:
    """C ファイルスコープにある最外周のプリプロセッサ条件分岐を探す。"""
    basic_mask = _blank_comments_and_literals(source)
    syntax_mask = _blank_preprocessor_lines(source, basic_mask)
    lines = _line_offsets(source)
    stack: list[dict[str, object]] = []
    regions: list[ConditionalRegion] = []
    brace_depth = 0

    for line_start, line_end in lines:
        line = basic_mask[line_start:line_end]
        directive = _CONDITIONAL_RE.match(line)
        if directive and brace_depth == 0:
            keyword = directive.group(1)
            if keyword in {"if", "ifdef", "ifndef"}:
                entry: dict[str, object] = {
                    "start": line_start,
                    "if_line_end": line_end,
                    "branches": [],
                    "branch_start": line_end,
                    "directive_start": None,
                    "directive_end": None,
                }
                stack.append(entry)
            elif keyword in {"elif", "else"} and stack:
                entry = stack[-1]
                branches = entry["branches"]
                assert isinstance(branches, list)
                branches.append(
                    (
                        entry["branch_start"],
                        line_start,
                        line_start,
                        line_end,
                    )
                )
                entry["branch_start"] = line_end
            elif keyword == "endif" and stack:
                entry = stack.pop()
                branches = entry["branches"]
                assert isinstance(branches, list)
                branches.append(
                    (
                        entry["branch_start"],
                        line_start,
                        None,
                        None,
                    )
                )
                if not stack:
                    regions.append(
                        ConditionalRegion(
                            start=entry["start"],  # type: ignore
                            if_line_end=entry["if_line_end"],  # type: ignore
                            branches=branches,
                            endif_start=line_start,
                            endif_end=line_end,
                        )
                    )

        masked_line = syntax_mask[line_start:line_end]
        brace_depth += masked_line.count("{") - masked_line.count("}")
        if brace_depth < 0:
            raise ValueError("条件分岐の解析中に閉じ括弧の対応が崩れています")

    if stack:
        raise ValueError("プリプロセッサの条件分岐が終了していません")
    return regions


def _is_header_prefix_line(masked_line: str) -> bool:
    stripped = masked_line.strip()
    if not stripped or stripped.startswith("#"):
        return False
    if any(character in stripped for character in "{};=()"):
        return False
    return stripped.startswith(("__attribute__", "__declspec")) or bool(
        re.match(r"[A-Za-z_]", stripped)
    )


def _is_block_comment_line(original_line: str) -> bool:
    stripped = original_line.lstrip()
    return stripped.startswith(("/*", "*", "*/", "//"))


def _expand_header_start(source: str, mask: str, candidate_start: int) -> int:
    """関数に隣接するコメントと属性だけの行を関数範囲に含める。"""
    line_start = source.rfind("\n", 0, candidate_start) + 1
    attached_lines = False
    while line_start > 0:
        previous_end = line_start - 1
        previous_start = source.rfind("\n", 0, previous_end) + 1
        original_line = source[previous_start:previous_end]
        masked_line = mask[previous_start:previous_end]
        if original_line.lstrip().startswith("#"):
            break
        if not original_line.strip():
            if attached_lines:
                break
            line_start = previous_start
            continue
        if _is_block_comment_line(original_line) or _is_header_prefix_line(masked_line):
            line_start = previous_start
            attached_lines = True
            continue
        break
    if not attached_lines:
        return source.rfind("\n", 0, candidate_start) + 1
    return line_start


def _function_match(masked_header: str) -> re.Match[str] | None:
    if "=" in masked_header:
        return None
    return _FUNCTION_RE.search(masked_header)


def _function_name(match: re.Match[str]) -> str:
    name = match.group("name")
    if name == "__not_in_flash_func":
        wrapper_match = re.search(
            r"__not_in_flash_func\s*\(\s*([A-Za-z_]\w*)\s*\)",
            match.group(0),
        )
        if wrapper_match:
            return wrapper_match.group(1)
    return name


def _signature(masked_header: str) -> str:
    return " ".join(masked_header.split())


def _contains_call(source: str, name: str) -> bool:
    mask = _blank_preprocessor_lines(source, _blank_comments_and_literals(source))
    return re.search(rf"\b{re.escape(name)}\s*\(", mask) is not None


def _find_matching_brace(mask: str, opening: int) -> int:
    depth = 0
    for index in range(opening, len(mask)):
        if mask[index] == "{":
            depth += 1
        elif mask[index] == "}":
            depth -= 1
            if depth == 0:
                return index + 1
    raise ValueError("関数本体が終了していません")


def _find_units(source: str) -> list[FunctionUnit]:
    basic_mask = _blank_comments_and_literals(source)
    mask = _blank_preprocessor_lines(source, basic_mask)
    units: list[FunctionUnit] = []
    statement_start = 0
    parenthesis_depth = 0
    brace_depth = 0
    index = 0

    while index < len(mask):
        current = mask[index]
        if current == "(":
            parenthesis_depth += 1
        elif current == ")" and parenthesis_depth:
            parenthesis_depth -= 1
        elif current == "{":
            if brace_depth == 0 and parenthesis_depth == 0:
                candidate = mask[statement_start:index]
                match = _function_match(candidate)
                if match:
                    header_start = _expand_header_start(
                        source, mask, statement_start + match.start()
                    )
                    body_end = _find_matching_brace(mask, index)
                    header = mask[header_start:index]
                    name = _function_name(match)
                    kind = "internal" if re.search(r"\bstatic\b", header) else "public"
                    units.append(
                        FunctionUnit(
                            start=header_start,
                            end=body_end,
                            kind=kind,
                            name=name,
                            signature=_signature(header) + ";",
                        )
                    )
                    index = body_end
                    statement_start = body_end
                    continue
            brace_depth += 1
        elif current == "}":
            if brace_depth:
                brace_depth -= 1
        elif current == ";" and brace_depth == 0 and parenthesis_depth == 0:
            candidate = mask[statement_start : index + 1]
            match = _function_match(candidate[:-1])
            if match and re.search(r"\bstatic\b", candidate):
                header_start = _expand_header_start(
                    source, mask, statement_start + match.start()
                )
                units.append(
                    FunctionUnit(
                        start=header_start,
                        end=index + 1,
                        kind="declaration",
                        name=_function_name(match),
                    )
                )
            statement_start = index + 1
        index += 1

    return sorted(units, key=lambda unit: unit.start)


def _section(title: str, blocks: list[str]) -> str:
    if not blocks:
        return ""
    body = "\n\n".join(block.rstrip() for block in blocks)
    return f"/*\n * {title}\n */\n\n{body}"


def _conditional_function_unit(
    source: str, region: ConditionalRegion
) -> FunctionUnit | None:
    inner_source = source[region.start : region.endif_end]
    units = _find_units(inner_source)
    definitions = [unit for unit in units if unit.kind != "declaration"]
    if not definitions or len({unit.kind for unit in definitions}) != 1:
        return None

    mask = list(
        _blank_preprocessor_lines(
            inner_source, _blank_comments_and_literals(inner_source)
        )
    )
    for unit in units:
        for index in range(unit.start, unit.end):
            if mask[index] != "\n":
                mask[index] = " "
    if "".join(mask).strip():
        return None

    basic_mask = _blank_comments_and_literals(source)
    generated_signatures = tuple(
        unit.signature for unit in definitions if unit.kind == "internal"
    )
    return FunctionUnit(
        start=_expand_header_start(source, basic_mask, region.start),
        end=region.endif_end,
        kind=definitions[0].kind,
        name=definitions[0].name,
        generated_signatures=generated_signatures,
    )


def _organize_plain_text(source: str, declared_names: set[str] | None = None) -> str:
    regions = _top_level_conditional_regions(source)
    movable_regions = {
        region.start: _conditional_function_unit(source, region) for region in regions
    }
    scan_source = list(source)
    conditional_units: list[FunctionUnit] = []
    for region in regions:
        unit = movable_regions[region.start]
        if unit is None:
            continue
        conditional_units.append(unit)
        for index in range(region.start, region.endif_end):
            if source[index] != "\n":
                scan_source[index] = " "

    units = _find_units("".join(scan_source)) + conditional_units
    units.sort(key=lambda unit: unit.start)
    if not units:
        return source

    declarations = [unit for unit in units if unit.kind == "declaration"]
    definitions = [unit for unit in units if unit.kind != "declaration"]
    if not definitions:
        return source
    public_definitions = [unit for unit in definitions if unit.kind == "public"]
    internal_definitions = [unit for unit in definitions if unit.kind == "internal"]

    existing_names = {unit.name for unit in declarations}
    if declared_names is not None:
        existing_names.update(declared_names)
    generated_declarations: list[str] = []
    for index, unit in enumerate(internal_definitions):
        if unit.name in existing_names:
            continue
        callers = public_definitions + internal_definitions[:index]
        if not any(
            _contains_call(source[caller.start : caller.end], unit.name)
            for caller in callers
        ):
            continue
        if unit.generated_signatures:
            generated_declarations.extend(unit.generated_signatures)
        elif unit.signature:
            generated_declarations.append(unit.signature)

    ranges = sorted((unit.start, unit.end) for unit in units)
    kept: list[str] = []
    previous_end = 0
    for start, end in ranges:
        if start < previous_end:
            raise ValueError("関数のソース範囲が重複しています")
        kept.append(source[previous_end:start])
        previous_end = end
    kept.append(source[previous_end:])
    prefix = "".join(kept).rstrip()

    declaration_blocks: list[str] = []
    for index, unit in enumerate(units):
        if unit.kind != "declaration":
            continue
        if index > 0 and units[index - 1].kind == "declaration":
            continue
        end_index = index + 1
        while end_index < len(units) and units[end_index].kind == "declaration":
            end_index += 1
        declaration_blocks.append(source[unit.start : units[end_index - 1].end])
    if generated_declarations:
        declaration_blocks.append("\n".join(generated_declarations))
    public_blocks = [source[unit.start : unit.end] for unit in public_definitions]
    internal_blocks = [source[unit.start : unit.end] for unit in internal_definitions]

    sections = [
        _section("内部関数宣言", declaration_blocks),
        _section("公開関数", public_blocks),
        _section("内部関数", internal_blocks),
    ]
    parts = [prefix] if prefix else []
    parts.extend(section for section in sections if section)
    result = "\n\n".join(parts)
    return result + "\n"


def _organize_conditional(
    source: str, region: ConditionalRegion, declared_names: set[str]
) -> str:
    output = [source[region.start : region.if_line_end]]
    for branch_start, branch_end, directive_start, directive_end in region.branches:
        branch_source = source[branch_start:branch_end]
        organized_branch = _organize_text(branch_source, declared_names)
        output.append(organized_branch)
        if organized_branch != branch_source and not organized_branch.endswith("\n\n"):
            output.append("\n")
        if directive_start is not None and directive_end is not None:
            output.append(source[directive_start:directive_end])
    output.append(source[region.endif_start : region.endif_end])
    return "".join(output)


def _organize_text(source: str, declared_names: set[str] | None = None) -> str:
    if declared_names is None:
        declared_names = {
            unit.name for unit in _find_units(source) if unit.kind == "declaration"
        }
    regions = _top_level_conditional_regions(source)
    if not regions:
        return _organize_plain_text(source, declared_names)

    region = regions[0]
    prefix = source[: region.start]
    suffix = source[region.endif_end :]
    if _conditional_function_unit(source, region) is not None:
        return _organize_plain_text(source, declared_names)
    organized_suffix = _organize_text(suffix, declared_names)
    if (
        organized_suffix != suffix
        and suffix[:1].isspace()
        and not organized_suffix.startswith(("\r", "\n"))
    ):
        organized_suffix = "\n" + organized_suffix
    return (
        _organize_text(prefix, declared_names)
        + _organize_conditional(source, region, declared_names)
        + organized_suffix
    )


def _remove_generated_sections(source: str) -> str:
    source = _GENERATED_SECTION_RE.sub("", source)
    return _LEGACY_SECTION_LINE_RE.sub("", source)


def organize_file(path: Path, write: bool) -> bool:
    original = path.read_text(encoding="utf-8")
    cleaned = _remove_generated_sections(original)
    declared_names = {
        unit.name for unit in _find_units(cleaned) if unit.kind == "declaration"
    }
    organized = _organize_text(cleaned, declared_names)
    changed = organized != original
    if changed and write:
        path.write_text(organized, encoding="utf-8", newline="")
    return changed


def _show_help(
    value: bool,
    context: typer.Context,
    _parameter: typer.CallbackParam,
) -> bool:
    if value:
        typer.echo(context.get_help())
        raise typer.Exit()
    return value


app = typer.Typer(
    help=__doc__ or "C 関数整理ツール。",
    add_completion=False,
    add_help_option=False,
)


@app.command()
def main(
    root: Annotated[
        Path,
        typer.Argument(help="C ファイルを整理する対象ディレクトリ。"),
    ],
    show_help: Annotated[
        bool,
        typer.Option(
            "--help",
            help="このヘルプを表示して終了する。",
            is_eager=True,
            callback=_show_help,
        ),
    ] = False,
    write: Annotated[
        bool,
        typer.Option("--write", help="整理結果を C ファイルへ書き込む。"),
    ] = False,
    check: Annotated[
        bool,
        typer.Option("--check", help="変更対象のファイルがあればエラーにする。"),
    ] = False,
) -> None:
    if not root.is_dir():
        typer.echo(f"エラー: ディレクトリが見つかりません: {root}", err=True)
        raise typer.Exit(code=2)

    changed_paths: list[Path] = []
    for path in sorted(root.rglob("*.c")):
        try:
            if organize_file(path, write=write):
                changed_paths.append(path)
        except (OSError, ValueError) as error:
            typer.echo(f"エラー: {path}: {error}", err=True)
            raise typer.Exit(code=2) from error

    for path in changed_paths:
        typer.echo(path)
    if check and changed_paths:
        raise typer.Exit(code=1)


if __name__ == "__main__":
    app()
