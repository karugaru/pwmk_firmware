from __future__ import annotations

import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from pwmk_profile import (
    current_profile_path,
    generate_profile,
    load_profile_config,
    profile_c_sources,
    select_profile,
    users_root,
)


class ProfileGenerationTest(unittest.TestCase):
    def setUp(self) -> None:
        profile_path = current_profile_path()
        self._original_current_profile = (
            profile_path.read_text(encoding="utf-8") if profile_path.exists() else None
        )
        self.addCleanup(self._restore_current_profile)

    def _restore_current_profile(self) -> None:
        profile_path = current_profile_path()
        if self._original_current_profile is None:
            if profile_path.exists():
                profile_path.unlink()
            return

        profile_path.write_text(self._original_current_profile, encoding="utf-8")

    def test_current_profile_is_valid(self) -> None:
        config = load_profile_config("remopicon_v1")

        self.assertEqual(config.cmake.board, "pico_w")
        self.assertEqual(config.board.rows, 5)
        self.assertEqual(config.board.cols, 5)
        self.assertEqual(len(config.keymap.keymap), config.board.active_layout_count)

    def test_generate_profile_outputs_expected_files(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_dir = Path(temporary_directory)
            generated_profile = generate_profile(build_dir, "remopicon_v1")

            self.assertEqual(generated_profile, "remopicon_v1")
            profile_cmake = build_dir / "generated" / "profile" / "profile.cmake"
            board_header = (
                build_dir / "generated" / "profile" / "src" / "settings" / "board.h"
            )
            keymap_header = (
                build_dir / "generated" / "profile" / "src" / "settings" / "keymap.h"
            )
            settings_header = (
                build_dir / "generated" / "profile" / "src" / "settings" / "settings.h"
            )

            self.assertTrue(profile_cmake.exists())
            self.assertTrue(board_header.exists())
            self.assertTrue(settings_header.exists())
            self.assertIn(
                'set(PICO_BOARD "pico_w"', profile_cmake.read_text(encoding="utf-8")
            )
            board_text = board_header.read_text(encoding="utf-8")
            keymap_text = keymap_header.read_text(encoding="utf-8")
            settings_text = settings_header.read_text(encoding="utf-8")

            self.assertIn("#define ROWS 5", board_text)
            self.assertIn("{ 0, 0 }", board_text)
            self.assertNotIn("{ row }", board_text)
            self.assertIn('#include "keyboard/code.h"', keymap_text)
            self.assertIn("#define BLE_PERSIST_SELECTED_SLOT 1", settings_text)

    def test_generate_profile_includes_profile_c_sources(self) -> None:
        profile_name = "test_profile_with_multiple_c_sources"
        profile_dir = users_root() / profile_name
        source_yaml = users_root() / "remopicon_v1" / "profile.yaml"

        if profile_dir.exists():
            shutil.rmtree(profile_dir)

        profile_dir.mkdir(parents=True)
        self.addCleanup(lambda: shutil.rmtree(profile_dir, ignore_errors=True))
        (profile_dir / "profile.yaml").write_text(
            source_yaml.read_text(encoding="utf-8"), encoding="utf-8"
        )
        (profile_dir / "alpha.c").write_text("void alpha(void) {}\n", encoding="utf-8")
        (profile_dir / "beta.c").write_text("void beta(void) {}\n", encoding="utf-8")

        with tempfile.TemporaryDirectory() as temporary_directory:
            build_dir = Path(temporary_directory)

            selected_profile = generate_profile(build_dir, profile_name)

            self.assertEqual(selected_profile, profile_name)
            self.assertEqual(
                [path.name for path in profile_c_sources(profile_name)],
                ["alpha.c", "beta.c"],
            )
            profile_cmake = build_dir / "generated" / "profile" / "profile.cmake"
            self.assertIn(
                "set(PWMK_PROFILE_C_SOURCES",
                profile_cmake.read_text(encoding="utf-8"),
            )
            self.assertIn("alpha.c", profile_cmake.read_text(encoding="utf-8"))
            self.assertIn("beta.c", profile_cmake.read_text(encoding="utf-8"))

    def test_generate_profile_allows_missing_c_sources(self) -> None:
        profile_name = "test_profile_without_c_sources"
        profile_dir = users_root() / profile_name
        source_yaml = users_root() / "remopicon_v1" / "profile.yaml"

        if profile_dir.exists():
            shutil.rmtree(profile_dir)

        profile_dir.mkdir(parents=True)
        self.addCleanup(lambda: shutil.rmtree(profile_dir, ignore_errors=True))
        (profile_dir / "profile.yaml").write_text(
            source_yaml.read_text(encoding="utf-8"), encoding="utf-8"
        )

        with tempfile.TemporaryDirectory() as temporary_directory:
            build_dir = Path(temporary_directory)

            selected_profile = generate_profile(build_dir, profile_name)

            self.assertEqual(selected_profile, profile_name)
            profile_cmake = build_dir / "generated" / "profile" / "profile.cmake"
            self.assertIn(
                "set(PWMK_PROFILE_C_SOURCES )",
                profile_cmake.read_text(encoding="utf-8"),
            )

    def test_select_profile_allows_missing_c_sources(self) -> None:
        profile_name = "test_profile_select_without_c_sources"
        profile_dir = users_root() / profile_name
        source_yaml = users_root() / "remopicon_v1" / "profile.yaml"

        if profile_dir.exists():
            shutil.rmtree(profile_dir)

        profile_dir.mkdir(parents=True)
        self.addCleanup(lambda: shutil.rmtree(profile_dir, ignore_errors=True))
        (profile_dir / "profile.yaml").write_text(
            source_yaml.read_text(encoding="utf-8"), encoding="utf-8"
        )

        with patch("pwmk_profile.clean_build_directory"):
            select_profile(profile_name)


if __name__ == "__main__":
    unittest.main()
