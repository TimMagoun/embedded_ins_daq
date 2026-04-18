#!/usr/bin/env python3

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import shutil
import os

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD_DIR = REPO_ROOT / "build_host_coverage"
SOURCE_DIR = REPO_ROOT / "host_tests"
MAIN_DIR = REPO_ROOT / "main"
COVERAGE_THRESHOLD = 90.0

JOBS = os.cpu_count()


def run(command: list[str]) -> None:
    subprocess.run(command, cwd=REPO_ROOT, check=True)


def configure_build() -> None:
    shutil.rmtree(BUILD_DIR, ignore_errors=True)
    run(
        [
            "cmake",
            "-S",
            str(SOURCE_DIR),
            "-B",
            str(BUILD_DIR),
            "-DENABLE_COVERAGE=ON",
        ]
    )


def build_and_test() -> None:
    run(["cmake", "--build", str(BUILD_DIR), "--parallel", str(JOBS)])
    run(
        [
            "ctest",
            "--test-dir",
            str(BUILD_DIR),
            "--output-on-failure",
            "--parallel",
            str(JOBS),
        ]
    )


def read_coverage_summary() -> float:
    completed = subprocess.run(
        [
            "gcovr",
            "-j",
            str(JOBS),
            "--root",
            str(REPO_ROOT),
            "--filter",
            str(MAIN_DIR),
            "--object-directory",
            str(BUILD_DIR),
            "--json-summary",
        ],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    summary = json.loads(completed.stdout)
    return float(summary["line_percent"])


def main() -> int:
    try:
        configure_build()
        build_and_test()
        line_coverage = read_coverage_summary()
    except subprocess.CalledProcessError as exc:
        return exc.returncode

    print(f"Host test line coverage: {line_coverage:.2f}%")
    if line_coverage <= COVERAGE_THRESHOLD:
        print(
            f"Coverage gate failed: expected > {COVERAGE_THRESHOLD:.2f}%, got {line_coverage:.2f}%",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
