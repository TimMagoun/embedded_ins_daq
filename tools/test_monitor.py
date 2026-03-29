from __future__ import annotations

import pathlib
import tempfile
import unittest
from unittest import mock

from tools import monitor as monitor_mod


class _FakeProcess:
    def __init__(self) -> None:
        self.pid = 1234
        self.returncode = 0
        self._poll_results = iter([None, None, 0])

    def poll(self) -> int | None:
        return next(self._poll_results, 0)


class MonitorStreamTests(unittest.TestCase):
    def test_stream_monitor_keeps_capturing_after_panic_marker(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            log_path = pathlib.Path(tmp_dir) / "monitor.log"
            process = _FakeProcess()

            with (
                mock.patch.object(monitor_mod.pty, "openpty", return_value=(10, 11)),
                mock.patch.object(
                    monitor_mod.subprocess, "Popen", return_value=process
                ),
                mock.patch.object(
                    monitor_mod.select,
                    "select",
                    side_effect=[
                        ([10], [], []),
                        ([10], [], []),
                        ([], [], []),
                        ([], [], []),
                    ],
                ),
                mock.patch.object(
                    monitor_mod,
                    "read_monitor_chunk",
                    side_effect=[
                        b"Guru Meditation Error: Core 0 panic'ed\n",
                        b"Backtrace: 0x40000000:0x00000000\n",
                    ],
                ),
                mock.patch.object(monitor_mod, "terminate_process_group"),
                mock.patch.object(monitor_mod, "normalize_log_file"),
                mock.patch.object(monitor_mod.os, "close"),
                mock.patch.object(monitor_mod.os, "write"),
                mock.patch.object(monitor_mod.sys.stdout, "flush"),
            ):
                monitor_mod.stream_monitor(
                    ["idf.py", "monitor"],
                    log_path,
                    ready_banner=None,
                    timeout_seconds=5,
                )

            self.assertIn("Backtrace:", log_path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
