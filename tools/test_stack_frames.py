from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import unittest

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
ELF_PATH = REPO_ROOT / "build" / "embedded_ins_daq.elf"
APP_MAIN_PATTERN = re.compile(r"<app_main>:\n[^\n]*addi\s+sp,sp,(-\d+)", re.MULTILINE)
MAX_APP_MAIN_STACK_FRAME_BYTES = 512


class StackFrameBudgetTests(unittest.TestCase):
    def test_app_main_stack_frame_stays_within_budget(self) -> None:
        objdump = shutil.which("riscv32-esp-elf-objdump")
        self.assertIsNotNone(objdump, "riscv32-esp-elf-objdump is required")
        self.assertTrue(ELF_PATH.is_file(), f"missing ELF: {ELF_PATH}")

        result = subprocess.run(
            [objdump, "-d", "--no-show-raw-insn", str(ELF_PATH)],
            check=True,
            capture_output=True,
            text=True,
        )

        match = APP_MAIN_PATTERN.search(result.stdout)
        self.assertIsNotNone(match, "app_main stack frame was not found in objdump")

        frame_bytes = abs(int(match.group(1)))
        self.assertLessEqual(
            frame_bytes,
            MAX_APP_MAIN_STACK_FRAME_BYTES,
            f"app_main stack frame is {frame_bytes} bytes",
        )


if __name__ == "__main__":
    unittest.main()
