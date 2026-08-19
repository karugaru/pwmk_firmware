from __future__ import annotations

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

from pwmk_common import ensure_linux, repo_root, safe_rmtree

PROJECT_NAME = "PWMK Firmware"
DEFAULT_OUTPUT = "site"
MERMAID_HTML = """<script id="pwmk-mermaid" src="https://cdn.jsdelivr.net/npm/mermaid@10.9.3/dist/mermaid.min.js" integrity="sha384-R63zfMfSwJF4xCR11wXii+QUsbiBIdiDzDbtxia72oGWfkT7WHJfmD/I/eeHPJyT" crossorigin="anonymous"></script>
<script id="pwmk-mermaid-renderer">
(() => {
    const mermaidStart = /^(flowchart|graph|sequenceDiagram|classDiagram|stateDiagram(?:-v2)?|erDiagram|journey|gantt|pie|gitGraph|mindmap|timeline|quadrantChart|xychart|block-beta)\\b/;
    const prefersDark = window.matchMedia("(prefers-color-scheme: dark)");
    const renderMermaid = () => {
        mermaid.initialize({
            startOnLoad: false,
            securityLevel: "strict",
            theme: prefersDark.matches ? "dark" : "default"
        });
        for (const fragment of document.querySelectorAll(".fragment")) {
            const source = [...fragment.querySelectorAll(".line")]
                .map((line) => line.textContent)
                .join("\\n");
            if (!mermaidStart.test(source.trim())) {
                continue;
            }
            const diagram = document.createElement("div");
            diagram.className = "mermaid";
            diagram.dataset.mermaidSource = source;
            diagram.textContent = source;
            fragment.replaceWith(diagram);
        }
        for (const diagram of document.querySelectorAll(".mermaid")) {
            diagram.textContent = diagram.dataset.mermaidSource;
        }
        mermaid.run({ querySelector: ".mermaid" });
    };
    prefersDark.addEventListener("change", renderMermaid);
    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", renderMermaid);
    } else {
        renderMermaid();
    }
})();
</script>"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="src/ と docs/ から Doxygen の静的ドキュメントサイトを生成する。"
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path(DEFAULT_OUTPUT),
        help=f"出力先ディレクトリ（既定値: {DEFAULT_OUTPUT}/）。",
    )
    parser.add_argument(
        "--doxygen",
        default="doxygen",
        help="使用するDoxygenコマンドまたは実行ファイルのパス。",
    )
    return parser.parse_args()


def quote_config_path(path: Path) -> str:
    return f'"{path.as_posix().replace(chr(34), r"\\\"")}"'


def build_doxyfile(root: Path, output: Path) -> str:
    input_paths = [root / "src", root / "docs", root / "README.md"]
    config = {
        "PROJECT_NAME": PROJECT_NAME,
        "PROJECT_BRIEF": "Raspberry Pi Pico W向けキーボードファームウェア",
        "OUTPUT_DIRECTORY": quote_config_path(output),
        "HTML_OUTPUT": ".",
        "INPUT": " ".join(quote_config_path(path) for path in input_paths),
        "FILE_PATTERNS": "*.c *.h *.md",
        "RECURSIVE": "YES",
        "EXCLUDE_PATTERNS": "*/build/* */.venv*/* */tmp/* */site/*",
        "IMAGE_PATH": quote_config_path(root / "docs" / "images"),
        "USE_MDFILE_AS_MAINPAGE": quote_config_path(root / "README.md"),
        "INPUT_ENCODING": "UTF-8",
        "EXTRACT_ALL": "YES",
        "EXTRACT_STATIC": "YES",
        "PREDEFINED": "PWMK_ENABLE_BLE=1 PWMK_ENABLE_USB=1",
        "SOURCE_BROWSER": "YES",
        "INLINE_SOURCES": "NO",
        "REFERENCES_RELATION": "YES",
        "GENERATE_TREEVIEW": "YES",
        "GENERATE_HTML": "YES",
        "GENERATE_LATEX": "NO",
        "GENERATE_MAN": "NO",
        "GENERATE_RTF": "NO",
        "GENERATE_XML": "NO",
        "HAVE_DOT": "NO",
        "WARN_IF_UNDOCUMENTED": "NO",
    }
    return "\n".join(f"{key} = {value}" for key, value in config.items()) + "\n"


def resolve_output(root: Path, output: Path) -> Path:
    resolved = output if output.is_absolute() else root / output
    resolved = resolved.resolve()
    protected_paths = ((root / "src").resolve(), (root / "docs").resolve())
    if resolved == root.resolve() or any(
        resolved == protected or protected in resolved.parents
        for protected in protected_paths
    ):
        raise SystemExit("出力先にリポジトリ本体、src/、docs/の配下は指定できません。")
    return resolved


def find_doxygen(command: str) -> str:
    executable = shutil.which(command)
    if executable is None:
        raise SystemExit("Doxygenが見つかりません。")
    return executable


def inject_mermaid_renderer(output: Path) -> None:
    for html_path in output.rglob("*.html"):
        html = html_path.read_text(encoding="utf-8")
        if 'id="pwmk-mermaid-renderer"' in html:
            continue
        if "</body>" not in html:
            continue
        html = html.replace("</body>", f"{MERMAID_HTML}\n</body>", 1)
        html_path.write_text(html, encoding="utf-8")


def generate_docs(root: Path, output: Path, doxygen: str) -> None:
    if output.exists():
        safe_rmtree(output)
    output.mkdir(parents=True)

    with tempfile.TemporaryDirectory(prefix="pwmk-doxygen-") as temporary_directory:
        doxyfile = Path(temporary_directory) / "Doxyfile"
        doxyfile.write_text(build_doxyfile(root, output), encoding="utf-8")
        subprocess.run([doxygen, str(doxyfile)], cwd=root, check=True)
    inject_mermaid_renderer(output)


def main() -> None:
    ensure_linux()
    args = parse_args()
    root = repo_root()
    output = resolve_output(root, args.output)
    doxygen = find_doxygen(args.doxygen)
    generate_docs(root, output, doxygen)
    print(f"ドキュメントサイトを生成しました: {output}")


if __name__ == "__main__":
    main()
