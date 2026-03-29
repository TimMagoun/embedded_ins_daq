from __future__ import annotations

import pathlib
import unittest

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
SDKCONFIG_DEFAULTS = REPO_ROOT / "sdkconfig.defaults"


class FatfsConfigTests(unittest.TestCase):
    def test_sdkconfig_defaults_enable_fatfs_long_filenames(self) -> None:
        text = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")

        self.assertIn("CONFIG_FATFS_LFN_HEAP=y", text)
        self.assertNotIn("CONFIG_FATFS_LFN_NONE=y", text)


if __name__ == "__main__":
    unittest.main()
