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

    def _copy_keyboard_layout(self, profile_dir: Path) -> None:
        source_layout = users_root() / "remopicon_v1" / "keyboard-layout.json"
        (profile_dir / "keyboard-layout.json").write_text(
            source_layout.read_text(encoding="utf-8"), encoding="utf-8"
        )

    def test_current_profile_is_valid(self) -> None:
        config = load_profile_config("remopicon_v1")

        self.assertEqual(config.cmake.board, "pico_w")
        self.assertEqual(config.board.rows, 5)
        self.assertEqual(config.board.cols, 5)
        self.assertEqual(config.settings.usb_vid, 0xCAFE)
        self.assertEqual(config.settings.usb_pid, 0x4001)
        self.assertEqual(len(config.keymap.keymap), config.board.layout_count)

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
        self._copy_keyboard_layout(profile_dir)

        with tempfile.TemporaryDirectory() as temporary_directory:
            build_dir = Path(temporary_directory)
            generate_profile(build_dir, profile_name)

            board_header = (
                build_dir / "generated" / "profile" / "src" / "settings" / "board.h"
            )
            self.assertIn(
                "#define LED_COUNT 2", board_header.read_text(encoding="utf-8")
            )

    def test_generate_profile_supports_vial_unlock_combo(self) -> None:
        profile_name = "test_profile_with_vial_unlock_combo"
        profile_data = {
            "cmake": {"board": "pico_w"},
            "board": {
                "rows_pins": [0, 1],
                "cols_pins": [2, 3],
                "gpio_sda_pin": 4,
                "gpio_scl_pin": 5,
                "gpio_dr_pin": 6,
                "gpio_led_pin": 7,
                "pin_settle_time_us": 1,
                "layout": [[0, 0], [0, 1], [1, 0], [1, 1]],
                "vial_unlock_combo": [[1, 1], [0, 1]],
            },
            "keymap": {"keymap": ["IKC_NOOP"] * 4},
            "settings": {
                "deep_sleep_timeout_seconds": 5,
                "led_brightness": 1,
                "debounce_time_ms": 0,
                "mouse_move_delta": 0,
                "mouse_move_thresh": 0,
                "mouse_wheel_delta": 0,
                "mouse_wheel_thresh": 0,
                "use_pinnacle": False,
                "pinnacle": {
                    "rotate": "PINNACLE_ROTATE_0",
                    "accel": 1.0,
                    "speed": 1.0,
                },
            },
        }

        with tempfile.TemporaryDirectory() as temporary_directory:
            temporary_root = Path(temporary_directory)
            profile_dir = temporary_root / "users" / profile_name
            profile_dir.mkdir(parents=True)
            (profile_dir / "profile.yaml").write_text(
                json.dumps(profile_data), encoding="utf-8"
            )
            (profile_dir / "keyboard-layout.json").write_text("[]\n", encoding="utf-8")

            with patch(
                "pwmk_profile.users_root", return_value=temporary_root / "users"
            ):
                build_dir = temporary_root / "build"
                generate_profile(build_dir, profile_name)

            settings_dir = build_dir / "generated" / "profile" / "src" / "settings"
            vial_definition_header = settings_dir / "vial_definition.h"

            self.assertIn(
                "#define VIAL_UNLOCK_COMBO_LENGTH 2",
                vial_definition_header.read_text(encoding="utf-8"),
            )
            self.assertIn(
                "{ 1, 1 }",
                vial_definition_header.read_text(encoding="utf-8"),
            )
            self.assertIn(
                "{ 0, 1 }",
                vial_definition_header.read_text(encoding="utf-8"),
            )

    def test_generate_profile_requires_keyboard_layout(self) -> None:
        profile_name = "test_profile_without_keyboard_layout"
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
            with self.assertRaisesRegex(
                SystemExit, "keyboard-layout.json が見つかりません"
            ):
                generate_profile(Path(temporary_directory), profile_name)

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

            self.assertTrue(profile_cmake.exists())
            self.assertTrue(board_header.exists())
            self.assertTrue(settings_header.exists())
            self.assertTrue(device_identity_header.exists())
            self.assertTrue(generated_gatt.exists())
            self.assertTrue(vial_definition_header.exists())
            for source_name in (
                "board.c",
                "keymap.c",
                "settings.c",
                "vial_definition.c",
            ):
                self.assertFalse((vial_definition_header.parent / source_name).exists())
            self.assertIn(
                'set(PICO_BOARD "pico_w"', profile_cmake.read_text(encoding="utf-8")
            )
            board_text = board_header.read_text(encoding="utf-8")
            keymap_text = keymap_header.read_text(encoding="utf-8")
            settings_text = settings_header.read_text(encoding="utf-8")
            device_identity_text = device_identity_header.read_text(encoding="utf-8")
            gatt_text = generated_gatt.read_text(encoding="utf-8")
            vial_definition_text = vial_definition_header.read_text(encoding="utf-8")

            self.assertIn("#define ROWS 5", board_text)
            self.assertIn("{ 0, 0 }", board_text)
            self.assertNotIn("{ row }", board_text)
            self.assertIn('#include "keyboard/code.h"', keymap_text)
            self.assertIn("#define USB_VID 0xCAFE", settings_text)
            self.assertIn("#define USB_PID 0x4001", settings_text)
            self.assertIn("#define BLE_PERSIST_SELECTED_SLOT 1", settings_text)
            self.assertIn("#define DEVICE_NAME", device_identity_text)
            self.assertIn('CHARACTERISTIC, GAP_DEVICE_NAME, READ, "', gatt_text)
            definition_bytes = bytes(
                int(value, 16)
                for value in re.findall(r"0x([0-9A-F]{2})", vial_definition_text)
            )
            decoded_definition = lzma.decompress(
                definition_bytes, format=lzma.FORMAT_ALONE
            ).decode("utf-8")
            definition = json.loads(decoded_definition)
            self.assertEqual(definition["matrix"], {"rows": 5, "cols": 5})
            expected_layout = json.loads(
                (users_root() / "remopicon_v1" / "keyboard-layout.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(definition["layouts"]["keymap"], expected_layout)

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
        self._copy_keyboard_layout(profile_dir)

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
        self._copy_keyboard_layout(profile_dir)
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
        self._copy_keyboard_layout(profile_dir)

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
