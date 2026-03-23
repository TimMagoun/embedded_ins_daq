#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import time

from tools.monitor import DeviceToolError, monitor

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_BUILD_DIR = REPO_ROOT / "build"
DEFAULT_RUNS_ROOT = REPO_ROOT / "artifacts" / "runs" / "device"
DEFAULT_LATEST_ROOT = REPO_ROOT / "artifacts" / "latest" / "device"
DECODE_PANIC = REPO_ROOT / "tools" / "decode_panic.sh"


class DeviceRunnerError(RuntimeError):
    pass


def require_idf_py() -> None:
    if shutil.which("idf.py") is None:
        raise DeviceRunnerError(
            "idf.py is not available in PATH. Run 'source ./tools/setup.sh' first."
        )


def resolve_port(arg_port: str | None) -> str:
    port = arg_port or os.environ.get("BOARD_PORT")
    if not port:
        raise DeviceRunnerError(
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


def make_timestamp() -> str:
    return time.strftime("%Y%m%d_%H%M%S", time.gmtime())


def copy_if_present(src: pathlib.Path, dest: pathlib.Path) -> None:
    if src.is_file():
        shutil.copy2(src, dest)


def copy_run_artifacts(run_dir: pathlib.Path, build_dir: pathlib.Path) -> None:
    artifacts = [
        (REPO_ROOT / "sdkconfig", run_dir / "sdkconfig"),
        (REPO_ROOT / "sdkconfig.defaults", run_dir / "sdkconfig.defaults"),
        (build_dir / "embedded_ins_daq.elf", run_dir / "embedded_ins_daq.elf"),
        (build_dir / "embedded_ins_daq.bin", run_dir / "embedded_ins_daq.bin"),
        (build_dir / "bootloader" / "bootloader.bin", run_dir / "bootloader.bin"),
        (
            build_dir / "partition_table" / "partition-table.bin",
            run_dir / "partition-table.bin",
        ),
        (build_dir / "flasher_args.json", run_dir / "flasher_args.json"),
        (build_dir / "project_description.json", run_dir / "project_description.json"),
    ]

    for src, dest in artifacts:
        copy_if_present(src, dest)


def write_metadata(path: pathlib.Path, values: dict[str, str]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for key, value in values.items():
            handle.write(f"{key}={value}\n")


def update_latest_link(link_path: pathlib.Path, target: pathlib.Path) -> None:
    if link_path.exists() or link_path.is_symlink():
        link_path.unlink()
    link_path.symlink_to(target)


def decode_panic_log(
    elf_path: pathlib.Path, monitor_log: pathlib.Path, output_path: pathlib.Path
) -> None:
    if not elf_path.is_file() or not monitor_log.is_file():
        return

    subprocess.run(
        [
            str(DECODE_PANIC),
            "--elf",
            str(elf_path),
            "--panic-log",
            str(monitor_log),
            "--output",
            str(output_path),
        ],
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        cwd=REPO_ROOT,
    )


def build_flash_command(build_dir: pathlib.Path, port: str) -> list[str]:
    return ["idf.py", "-B", str(build_dir), "-p", port, "flash"]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Flash the current build, run monitor.py, and capture case artifacts."
    )
    parser.add_argument("--case", required=True)
    parser.add_argument("--port")
    parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIR))
    parser.add_argument("--ready-banner")
    parser.add_argument("--timeout", type=int, default=30)
    return parser


def run_case(args: argparse.Namespace) -> int:
    require_idf_py()

    build_dir = repo_path(args.build_dir, DEFAULT_BUILD_DIR)
    if not build_dir.is_dir():
        raise DeviceRunnerError(f"Missing build directory: {build_dir}")

    case_name = args.case
    port = resolve_port(args.port)
    ready_banner = args.ready_banner or f"READY: {case_name}"

    timestamp = make_timestamp()
    run_dir = DEFAULT_RUNS_ROOT / case_name / timestamp
    latest_link = DEFAULT_LATEST_ROOT / case_name
    monitor_log = run_dir / "monitor.log"
    decode_log = run_dir / "decoded_backtrace.txt"

    run_dir.mkdir(parents=True, exist_ok=True)
    DEFAULT_LATEST_ROOT.mkdir(parents=True, exist_ok=True)

    flash = subprocess.run(
        build_flash_command(build_dir, port), cwd=REPO_ROOT, check=False
    )
    if flash.returncode != 0:
        write_metadata(
            run_dir / "metadata.txt",
            {
                "case": case_name,
                "port": port,
                "build_dir": str(build_dir),
                "ready_banner": ready_banner,
                "timeout_seconds": str(args.timeout),
                "timestamp_utc": timestamp,
                "flash_exit_code": str(flash.returncode),
                "monitor_exit_code": "",
            },
        )
        copy_run_artifacts(run_dir, build_dir)
        update_latest_link(
            latest_link, pathlib.Path("../../runs/device") / case_name / timestamp
        )
        return flash.returncode

    monitor_exit_code = monitor(
        build_dir=build_dir,
        port=port,
        ready_banner=ready_banner,
        timeout=args.timeout,
        log=monitor_log,
        monitor_args=args.monitor_args,
    )

    copy_run_artifacts(run_dir, build_dir)
    decode_panic_log(run_dir / "embedded_ins_daq.elf", monitor_log, decode_log)
    write_metadata(
        run_dir / "metadata.txt",
        {
            "case": case_name,
            "port": port,
            "build_dir": str(build_dir),
            "ready_banner": ready_banner,
            "timeout_seconds": str(args.timeout),
            "timestamp_utc": timestamp,
            "flash_exit_code": str(flash.returncode),
            "monitor_exit_code": str(monitor_exit_code),
        },
    )
    update_latest_link(
        latest_link, pathlib.Path("../../runs/device") / case_name / timestamp
    )

    if monitor_exit_code == 0:
        print(f"Case {case_name} reached ready banner. Artifacts: {run_dir}")
    elif monitor_exit_code == 3:
        print(
            f"Case {case_name} entered GDBStub before the ready banner. Artifacts: {run_dir}",
            file=sys.stderr,
        )
    elif monitor_exit_code == 2:
        print(
            f"Case {case_name} captured panic output before the ready banner. Artifacts: {run_dir}",
            file=sys.stderr,
        )
    elif monitor_exit_code == 1:
        print(
            f'Case {case_name} timed out waiting for ready banner "{ready_banner}". Artifacts: {run_dir}',
            file=sys.stderr,
        )

    return monitor_exit_code


def main() -> int:
    parser = build_parser()
    args, extra_args = parser.parse_known_args()
    args.monitor_args = [arg for arg in extra_args if arg != "--"]

    try:
        return run_case(args)
    except DeviceRunnerError as exc:
        print(f"[run-case] ERROR: {exc}", file=sys.stderr)
        return 1
    except DeviceToolError as exc:
        print(f"[run-case] ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
