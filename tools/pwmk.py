from __future__ import annotations

import typer

from pwmk_build import register_build_command
from pwmk_profile import register_profile_commands

app = typer.Typer(help="PWMK 用 CLI。", add_completion=False, no_args_is_help=True)


@app.callback()
def root_command() -> None:
    """
    PWMK CLI のルートコマンド。
    """


register_build_command(app)
register_profile_commands(app)

if __name__ == "__main__":
    app(prog_name="pwmk")
