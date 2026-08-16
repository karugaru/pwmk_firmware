from __future__ import annotations

import unittest

from tools.test_build import (
    BUILD_TEST_PROFILES,
    BuildTestTarget,
    build_test_matrix,
    shell_command,
)


class BuildTestMatrixTest(unittest.TestCase):
    def test_build_test_matrix_combines_each_target_and_profile(self) -> None:
        target = BuildTestTarget(
            name="example_linux",
            image="example:latest",
            bootstrap_command="bootstrap",
        )

        test_cases = build_test_matrix([target])

        self.assertEqual(
            [(test_case.target, test_case.profile) for test_case in test_cases],
            [(target, profile) for profile in BUILD_TEST_PROFILES],
        )

    def test_shell_command_uses_the_case_profile(self) -> None:
        target = BuildTestTarget(
            name="example_linux",
            image="example:latest",
            bootstrap_command="bootstrap",
        )
        test_case = build_test_matrix([target])[1]

        command = shell_command(test_case)

        self.assertIn(f"--profile {test_case.profile}", command)

    def test_shell_command_does_not_preserve_unsupported_metadata(self) -> None:
        target = BuildTestTarget(
            name="example_linux",
            image="example:latest",
            bootstrap_command="bootstrap",
        )
        test_case = build_test_matrix([target])[0]

        command = shell_command(test_case)

        self.assertIn("cp -r --no-preserve=all /workspace/.", command)


if __name__ == "__main__":
    unittest.main()
