from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from organize_c_functions import organize_file


class OrganizeCFunctionsTest(unittest.TestCase):
    def _assert_organized(self, source: str, expected: str) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "sample.c"
            path.write_text(source, encoding="utf-8")

            self.assertTrue(organize_file(path, write=True))
            self.assertEqual(expected, path.read_text(encoding="utf-8"))

            self.assertFalse(organize_file(path, write=True))
            self.assertEqual(expected, path.read_text(encoding="utf-8"))

    def test_organizes_varied_sources_and_is_idempotent(self) -> None:
        cases = {
            "plain functions": (
                """#include "feature.h"

static void helper(void);

void public_late(void) {
  helper();
}

static void helper(void) {
  const char *text = "fake(void) { not_a_function(); }";
  (void)text;
}

void public_early(void) {
  helper();
}
""",
                """#include "feature.h"

/*
 * 内部関数宣言
 */

static void helper(void);

/*
 * 公開関数
 */

void public_late(void) {
  helper();
}

void public_early(void) {
  helper();
}

/*
 * 内部関数
 */

static void helper(void) {
  const char *text = "fake(void) { not_a_function(); }";
  (void)text;
}
""",
            ),
            "attributed functions": (
                """#include "feature.h"

__declspec(noinline)
static void helper(void) {
}

__attribute__((weak)) void weak_entry(void) {}

void public_entry(void) {
  helper();
}
""",
                """#include "feature.h"

/*
 * 内部関数宣言
 */

__declspec(noinline) static void helper(void);

/*
 * 公開関数
 */

__attribute__((weak)) void weak_entry(void) {}

void public_entry(void) {
  helper();
}

/*
 * 内部関数
 */

__declspec(noinline)
static void helper(void) {
}
""",
            ),
            "conditional branches": (
                """#include "feature.h"

#ifdef FEATURE_ENABLED
static void feature_helper(void) {}
void feature_entry(void) { feature_helper(); }
#else
void fallback_entry(void) {}
#endif

static void common_helper(void) {}
void common_entry(void) { common_helper(); }
""",
                """#include "feature.h"

#ifdef FEATURE_ENABLED
/*
 * 内部関数宣言
 */

static void feature_helper(void);

/*
 * 公開関数
 */

void feature_entry(void) { feature_helper(); }

/*
 * 内部関数
 */

static void feature_helper(void) {}

#else
/*
 * 公開関数
 */

void fallback_entry(void) {}

#endif

/*
 * 内部関数宣言
 */

static void common_helper(void);

/*
 * 公開関数
 */

void common_entry(void) { common_helper(); }

/*
 * 内部関数
 */

static void common_helper(void) {}
""",
            ),
        }

        for name, (source, expected) in cases.items():
            with self.subTest(name=name):
                self._assert_organized(source, expected)

    def test_leaves_non_function_source_unchanged(self) -> None:
        source = """#include "feature.h"

#define DECLARE_FUNCTION(name) void name(void)
const char *message = "fake(void) { not_a_function(); }";
"""

        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "sample.c"
            path.write_text(source, encoding="utf-8")

            self.assertFalse(organize_file(path, write=True))
            self.assertEqual(source, path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
