from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from pwmk_build_prep import BuildCommandArgs, clean_build_directory, delete_cached_repos
from pwmk_common import repo_root, safe_rmtree


class SafeRmtreeTest(unittest.TestCase):
    def test_deletes_directory_inside_repository(self) -> None:
        with tempfile.TemporaryDirectory(dir=repo_root()) as temporary_directory:
            target = Path(temporary_directory) / "target"
            target.mkdir()
            (target / "file.txt").write_text("test", encoding="utf-8")

            safe_rmtree(target)

            self.assertFalse(target.exists())

    def test_rejects_directory_outside_repository(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            target = Path(temporary_directory) / "target"
            target.mkdir()

            with self.assertRaises(ValueError):
                safe_rmtree(target)

            self.assertTrue(target.exists())

    def test_rejects_directory_that_resolves_outside_repository(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            outside_target = Path(temporary_directory) / "target"
            outside_target.mkdir()
            path_with_parent_reference = repo_root() / ".." / outside_target.name

            with self.assertRaises(ValueError):
                safe_rmtree(path_with_parent_reference)

            self.assertTrue(outside_target.exists())

    def test_rejects_repository_root(self) -> None:
        with self.assertRaises(ValueError):
            safe_rmtree(repo_root())

    def test_allows_directory_inside_pwmk_cache(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory) / ".pwmk"
            target = cache_root / "pico-sdk-2.3.0"
            target.mkdir(parents=True)

            with patch("pwmk_common.pwmk_cache_root", return_value=cache_root):
                safe_rmtree(target, allow_pwmk_cache=True)

            self.assertFalse(target.exists())

    def test_allows_dependency_cache_deletion_inside_pwmk_cache(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory) / ".pwmk"
            sdk_cache = cache_root / "pico-sdk-2.3.0"
            picotool_cache = cache_root / "picotool-2.3.0"
            sdk_cache.mkdir(parents=True)
            picotool_cache.mkdir(parents=True)

            with patch("pwmk_build_prep.pwmk_cache_root", return_value=cache_root):
                with patch("pwmk_common.pwmk_cache_root", return_value=cache_root):
                    delete_cached_repos(BuildCommandArgs())

            self.assertFalse(sdk_cache.exists())
            self.assertFalse(picotool_cache.exists())

    def test_rejects_external_dependency_cache_deletion(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            sdk_cache = Path(temporary_directory) / "pico-sdk-2.3.0"
            sdk_cache.mkdir()

            with patch("pwmk_build_prep.sdk_cache_dir", return_value=sdk_cache):
                with self.assertRaises(SystemExit):
                    delete_cached_repos(BuildCommandArgs())

            self.assertTrue(sdk_cache.exists())

    def test_deletes_repository_build_directory_without_confirmation(self) -> None:
        with tempfile.TemporaryDirectory(dir=repo_root()) as temporary_directory:
            target = Path(temporary_directory) / "build"
            target.mkdir()

            with patch("pwmk_build_prep.typer.confirm") as confirm:
                clean_build_directory(target)

            confirm.assert_not_called()
            self.assertFalse(target.exists())

    def test_confirms_before_deleting_external_build_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            target = Path(temporary_directory) / "build"
            target.mkdir()

            with patch("pwmk_build_prep.typer.confirm", return_value=False) as confirm:
                with self.assertRaises(SystemExit):
                    clean_build_directory(target)

            confirm.assert_called_once()
            self.assertTrue(target.exists())

    def test_deletes_external_build_directory_after_confirmation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            target = Path(temporary_directory) / "build"
            target.mkdir()

            with patch("pwmk_build_prep.typer.confirm", return_value=True) as confirm:
                clean_build_directory(target)

            confirm.assert_called_once()
            self.assertFalse(target.exists())


if __name__ == "__main__":
    unittest.main()
