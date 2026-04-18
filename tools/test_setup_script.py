from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


class SetupScriptTest(unittest.TestCase):
    def test_setup_script_sources_from_any_cwd_in_bash_and_zsh(self) -> None:
        repo_root = Path(__file__).resolve().parents[1]
        setup_script = repo_root / "tools" / "setup.sh"

        with tempfile.TemporaryDirectory() as tmpdir:
            for shell in ("bash", "zsh"):
                completed = subprocess.run(
                    [
                        shell,
                        "-lc",
                        f"source {setup_script} >/dev/null && printf OK",
                    ],
                    cwd=tmpdir,
                    capture_output=True,
                    text=True,
                    check=False,
                )

                self.assertEqual(
                    completed.returncode,
                    0,
                    msg=f"{shell} failed:\nSTDOUT={completed.stdout}\nSTDERR={completed.stderr}",
                )
                self.assertEqual(completed.stdout, "OK")


if __name__ == "__main__":
    unittest.main()
