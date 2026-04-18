from __future__ import annotations

import unittest
from pathlib import Path

from tools.run_cppcheck import build_cppcheck_command
from tools.run_cppcheck import filter_entries


class RunCppcheckTest(unittest.TestCase):
    def test_filter_entries_keeps_only_main_sources(self) -> None:
        entries = [
            {"file": "/repo/main/app_main.cpp"},
            {"file": "/repo/main/daq_state.cpp"},
            {"file": "/repo/host_tests/test_project_skeleton.cpp"},
            {"file": "/repo/esp-idf/components/log/log.c"},
        ]

        filtered = filter_entries(entries, Path("/repo/main"))

        self.assertEqual(
            filtered,
            [
                {"file": "/repo/main/app_main.cpp"},
                {"file": "/repo/main/daq_state.cpp"},
            ],
        )

    def test_build_cppcheck_command_uses_project_and_cpp20_compat_define(self) -> None:
        command = build_cppcheck_command(
            project_file=Path("/tmp/compile_commands.json"),
            strict=True,
        )

        self.assertEqual(command[0], "cppcheck")
        self.assertIn("--project=/tmp/compile_commands.json", command)
        self.assertIn("-D__cplusplus=201703L", command)
        self.assertIn("--enable=style,information,missingInclude", command)


if __name__ == "__main__":
    unittest.main()
