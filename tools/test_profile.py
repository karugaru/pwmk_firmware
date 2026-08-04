from __future__ import annotations

import json
import lzma
import re
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
        self.assertEqual(config.settings.usb_vid, 0xCAFE)
        self.assertEqual(config.settings.usb_pid, 0x4001)
        self.assertEqual(len(config.keymap.keymap), config.board.active_layout_count)

    def test_generate_profile_supports_led_count(self) -> None:
        profile_name = "test_profile_with_multiple_leds"
        profile_dir = users_root() / profile_name
        source_yaml = users_root() / "remopicon_v1" / "profile.yaml"

        if profile_dir.exists():
            shutil.rmtree(profile_dir)

        profile_dir.mkdir(parents=True)
        self.addCleanup(lambda: shutil.rmtree(profile_dir, ignore_errors=True))
        yaml_text = source_yaml.read_text(encoding="utf-8").replace(
            "  gpio_led_pin: 16",
            "  gpio_led_pin: 16\n  led_count: 2",
        )
        (profile_dir / "profile.yaml").write_text(yaml_text, encoding="utf-8")

        with tempfile.TemporaryDirectory() as temporary_directory:
            build_dir = Path(temporary_directory)
            generate_profile(build_dir, profile_name)

            board_header = (
                build_dir / "generated" / "profile" / "src" / "settings" / "board.h"
            )
            self.assertIn(
                "#define LED_COUNT 2", board_header.read_text(encoding="utf-8")
            )

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
            device_identity_header = (
                build_dir / "generated" / "profile" / "src" / "device_identity.h"
            )
            generated_gatt = build_dir / "generated" / "profile" / "pwmk.gatt"
            vial_definition_header = (
                build_dir
                / "generated"
                / "profile"
                / "src"
                / "settings"
                / "vial_definition.h"
            )
            vial_definition_source = (
                build_dir
                / "generated"
                / "profile"
                / "src"
                / "settings"
                / "vial_definition.c"
            )

            self.assertTrue(profile_cmake.exists())
            self.assertTrue(board_header.exists())
            self.assertTrue(settings_header.exists())
            self.assertTrue(device_identity_header.exists())
            self.assertTrue(generated_gatt.exists())
            self.assertTrue(vial_definition_header.exists())
            self.assertTrue(vial_definition_source.exists())
            self.assertIn(
                'set(PICO_BOARD "pico_w"', profile_cmake.read_text(encoding="utf-8")
            )
            board_text = board_header.read_text(encoding="utf-8")
            keymap_text = keymap_header.read_text(encoding="utf-8")
            settings_text = settings_header.read_text(encoding="utf-8")
            device_identity_text = device_identity_header.read_text(encoding="utf-8")
            gatt_text = generated_gatt.read_text(encoding="utf-8")
            vial_definition_text = vial_definition_source.read_text(encoding="utf-8")

            self.assertIn("#define ROWS 5", board_text)
            self.assertIn("{ 0, 0 }", board_text)
            self.assertNotIn("{ row }", board_text)
            self.assertIn('#include "keyboard/code.h"', keymap_text)
            self.assertIn("#define USB_VID 0xCAFE", settings_text)
            self.assertIn("#define USB_PID 0x4001", settings_text)
            self.assertIn("#define BLE_PERSIST_SELECTED_SLOT 1", settings_text)
            self.assertIn("#define DEVICE_NAME", device_identity_text)
            self.assertIn('CHARACTERISTIC, GAP_DEVICE_NAME, READ, "', gatt_text)
            definition_array = re.search(
                r"const uint8_t vial_keyboard_definition\[[^]]+\] = \{(?P<bytes>.*?)\n\};",
                vial_definition_text,
                re.DOTALL,
            )
            self.assertIsNotNone(definition_array)
            assert definition_array is not None
            definition_bytes = bytes(
                int(value, 16)
                for value in re.findall(r"0x([0-9A-F]{2})", definition_array["bytes"])
            )
            decoded_definition = lzma.decompress(
                definition_bytes, format=lzma.FORMAT_ALONE
            ).decode("utf-8")
            comment_marker = (
                "// Vial keyboard definition before UTF-8 encoding and LZMA compression:"
            )
            comment_start = vial_definition_text.splitlines().index(comment_marker) + 1
            comment_definition = "\n".join(
                line[3:]
                for line in vial_definition_text.splitlines()[comment_start:]
                if line.startswith("// ")
            )
            self.assertEqual(comment_definition, decoded_definition)
            definition = json.loads(decoded_definition)
            self.assertEqual(definition["matrix"], {"rows": 5, "cols": 5})
            self.assertEqual(definition["layouts"]["keymap"][0][0], "0,0")

    def test_generate_profile_supports_usb_identifier_overrides(self) -> None:
        profile_name = "test_profile_with_usb_identifier_overrides"
        profile_dir = users_root() / profile_name
        source_yaml = users_root() / "remopicon_v1" / "profile.yaml"

        if profile_dir.exists():
            shutil.rmtree(profile_dir)

        profile_dir.mkdir(parents=True)
        self.addCleanup(lambda: shutil.rmtree(profile_dir, ignore_errors=True))
        yaml_text = source_yaml.read_text(encoding="utf-8").replace(
            "  usb_vid: 0xCAFE\n  usb_pid: 0x4001",
            "  usb_vid: 0x1234\n  usb_pid: 0xABCD",
        )
        (profile_dir / "profile.yaml").write_text(yaml_text, encoding="utf-8")

        with tempfile.TemporaryDirectory() as temporary_directory:
            build_dir = Path(temporary_directory)
            generate_profile(build_dir, profile_name)

            settings_header = (
                build_dir / "generated" / "profile" / "src" / "settings" / "settings.h"
            )
            settings_text = settings_header.read_text(encoding="utf-8")
            self.assertIn("#define USB_VID 0x1234", settings_text)
            self.assertIn("#define USB_PID 0xABCD", settings_text)

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
