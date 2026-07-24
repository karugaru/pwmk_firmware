from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from pwmk_profile import generate_profile, load_profile_config


class ProfileGenerationTest(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
