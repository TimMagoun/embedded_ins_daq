#!/usr/bin/env python3

from __future__ import annotations

import argparse
import codecs
import errno
import os
import pathlib
import pty
import re
import select
import shutil
import signal
import subprocess
import sys
import time
from dataclasses import dataclass

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_BUILD_DIR = REPO_ROOT / "build"
DEFAULT_LOG_PATH = REPO_ROOT / "artifacts" / "latest" / "device" / "monitor.log"
PANIC_PATTERN = re.compile(r"Guru Meditation Error|Backtrace:|abort\(\) was called")
GDBSTUB_PATTERN = re.compile(
    r"Entering gdb stub now\.|(?:^|\n|\+)\$T[0-9a-fA-F]{2}[^\n]*#[0-9a-fA-F]{2}"
)
ANSI_ESCAPE_PATTERN = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
ANSI_FRAGMENT_PATTERN = re.compile(r"\[(?:\d{1,3}(?:;\d{1,3})*)?[A-Za-z]")
GDBSTUB_EXIT_CODE = 3


class DeviceToolError(RuntimeError):
    pass


@dataclass
class MonitorResult:
    returncode: int = 0
    ready_seen: bool = False
    panic_seen: bool = False
    gdbstub_seen: bool = False
    timed_out: bool = False


def require_idf_py() -> None:
    if shutil.which("idf.py") is None:
        raise DeviceToolError(
            "idf.py is not available in PATH. Run 'source ./tools/setup.sh' first."
        )


def resolve_port(arg_port: str | None) -> str:
    port = arg_port or os.environ.get("BOARD_PORT")
    if not port:
        raise DeviceToolError(
            "No serial port configured. Run 'source ./tools/setup.sh' or pass --port."
        )
    return port


def repo_path(value: str | None, default: pathlib.Path) -> pathlib.Path:
    if not value:
        return default
    path = pathlib.Path(value)
    if not path.is_absolute():
        path = REPO_ROOT / path
    return path.resolve()


def terminate_process_group(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return

    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            os.killpg(process.pid, sig)
        except ProcessLookupError:
            return
        try:
            process.wait(timeout=2)
            return
        except subprocess.TimeoutExpired:
            pass

    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        return
    process.wait(timeout=2)


def sanitize_for_log(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = ANSI_ESCAPE_PATTERN.sub("", text)
    text = ANSI_FRAGMENT_PATTERN.sub("", text)
    cleaned: list[str] = []
    for char in text:
        if char == "\ufffd":
            continue
        if char == "\n" or char == "\t" or ord(char) >= 32:
            cleaned.append(char)
    return "".join(cleaned)


def normalize_log_file(path: pathlib.Path) -> None:
    text = path.read_text(encoding="utf-8", errors="replace")
    path.write_text(sanitize_for_log(text), encoding="utf-8")


def read_monitor_chunk(master_fd: int) -> bytes | None:
    try:
        chunk = os.read(master_fd, 4096)
    except OSError as exc:
        if exc.errno == errno.EIO:
            return None
        raise
    return chunk or None


def update_state(
    search_buffer: str, text: str, ready_banner: str | None, result: MonitorResult
) -> str:
    search_buffer = (search_buffer + text)[-16384:]
    if ready_banner and ready_banner in search_buffer:
        result.ready_seen = True
    if PANIC_PATTERN.search(search_buffer):
        result.panic_seen = True
    if GDBSTUB_PATTERN.search(search_buffer):
        result.gdbstub_seen = True
    return search_buffer


def build_monitor_command(
    build_dir: pathlib.Path,
    port: str,
    *,
    extra_args: list[str],
) -> list[str]:
    command = [
        "idf.py",
        "-B",
        str(build_dir),
        "-p",
        port,
        "monitor",
        "--disable-auto-color",
    ]
    command.extend(extra_args)
    return command


def stream_monitor(
    command: list[str],
    log_path: pathlib.Path,
    *,
    ready_banner: str | None,
    timeout_seconds: int | None,
) -> MonitorResult:
    result = MonitorResult()
    deadline = None if timeout_seconds is None else time.monotonic() + timeout_seconds
    decoder = codecs.getincrementaldecoder("utf-8")("replace")
    search_buffer = ""
    master_fd, slave_fd = pty.openpty()

    with log_path.open("w", encoding="utf-8") as log_handle:
        process = subprocess.Popen(
            command,
            cwd=str(REPO_ROOT),
            stdin=slave_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            start_new_session=True,
        )
        os.close(slave_fd)

        try:
            while True:
                timeout = 0.2
                if deadline is not None:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        result.timed_out = True
                        break
                    timeout = min(timeout, remaining)

                ready, _, _ = select.select([master_fd], [], [], timeout)
                if ready:
                    chunk = read_monitor_chunk(master_fd)
                    if chunk is None:
                        break

                    os.write(sys.stdout.fileno(), chunk)
                    sys.stdout.flush()

                    text = decoder.decode(chunk)
                    log_handle.write(text)
                    log_handle.flush()

                    search_buffer = update_state(
                        search_buffer, text, ready_banner, result
                    )
                    if result.ready_seen or result.panic_seen or result.gdbstub_seen:
                        break

                if process.poll() is not None and not ready:
                    break
        finally:
            tail = decoder.decode(b"", final=True)
            if tail:
                log_handle.write(tail)
                log_handle.flush()

            terminate_process_group(process)
            os.close(master_fd)

        result.returncode = process.returncode if process.returncode is not None else 0

    normalize_log_file(log_path)
    return result


def monitor_exit_code(result: MonitorResult, ready_banner: str | None) -> int:
    if result.ready_seen:
        return 0
    if result.gdbstub_seen:
        return GDBSTUB_EXIT_CODE
    if result.panic_seen:
        return 2
    if result.timed_out:
        return 1
    if result.returncode != 0:
        return result.returncode
    if ready_banner:
        return 1
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Thin ESP-IDF monitor wrapper for automation."
    )
    parser.add_argument("--port")
    parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIR))
    parser.add_argument("--ready-banner")
    parser.add_argument("--timeout", type=int)
    parser.add_argument("--log", default=str(DEFAULT_LOG_PATH))
    return parser


def monitor(
    *,
    build_dir: str | pathlib.Path = DEFAULT_BUILD_DIR,
    port: str | None = None,
    ready_banner: str | None = None,
    timeout: int | None = None,
    log: str | pathlib.Path = DEFAULT_LOG_PATH,
    monitor_args: list[str] | None = None,
) -> int:
    require_idf_py()

    resolved_build_dir = repo_path(str(build_dir), DEFAULT_BUILD_DIR)
    resolved_port = resolve_port(port)
    log_path = repo_path(str(log), DEFAULT_LOG_PATH)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    command = build_monitor_command(
        resolved_build_dir,
        resolved_port,
        extra_args=list(monitor_args or []),
    )

    result = stream_monitor(
        command,
        log_path,
        ready_banner=ready_banner,
        timeout_seconds=timeout,
    )
    return monitor_exit_code(result, ready_banner)


def main() -> int:
    parser = build_parser()
    args, extra_args = parser.parse_known_args()
    monitor_args = [arg for arg in extra_args if arg != "--"]

    try:
        return monitor(
            build_dir=args.build_dir,
            port=args.port,
            ready_banner=args.ready_banner,
            timeout=args.timeout,
            log=args.log,
            monitor_args=monitor_args,
        )
    except DeviceToolError as exc:
        print(f"[device-tools] ERROR: {exc}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as exc:
        print(
            f"[device-tools] ERROR: command failed with exit code {exc.returncode}",
            file=sys.stderr,
        )
        if exc.stderr:
            stderr = exc.stderr.strip()
            if stderr:
                print(stderr, file=sys.stderr)
        return exc.returncode or 1


if __name__ == "__main__":
    sys.exit(main())
