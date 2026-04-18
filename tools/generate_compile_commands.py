#!/usr/bin/env python3

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
from typing import Any

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]


def host_build_dir() -> pathlib.Path:
    return REPO_ROOT / "build_host"


def esp_build_dir() -> pathlib.Path:
    return REPO_ROOT / "build"


def output_file() -> pathlib.Path:
    return REPO_ROOT / "compile_commands.json"


def run_command(command: list[str], *, cwd: pathlib.Path) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def build_host_project() -> None:
    run_command(
        [
            "cmake",
            "-S",
            str(REPO_ROOT / "host_tests"),
            "-B",
            str(host_build_dir()),
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ],
        cwd=REPO_ROOT,
    )
    run_command(["cmake", "--build", str(host_build_dir())], cwd=REPO_ROOT)


def build_esp32_project() -> None:
    run_command(
        [
            "bash",
            "-lc",
            "source tools/setup.sh >/dev/null 2>&1 && idf.py build",
        ],
        cwd=REPO_ROOT,
    )


def load_compile_commands(path: pathlib.Path) -> list[dict[str, Any]]:
    if not path.is_file():
        raise FileNotFoundError(f"Missing compile commands file: {path}")

    with path.open(encoding="utf-8") as handle:
        entries = json.load(handle)

    if not isinstance(entries, list):
        raise ValueError(f"Expected a list of compile command entries in {path}")

    normalized: list[dict[str, Any]] = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError(f"Expected object entries in {path}")
        normalized.append(entry)

    return normalized


def merge_compile_commands(
    host_entries: list[dict[str, Any]],
    esp_entries: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    merged: list[dict[str, Any]] = []
    seen_files: set[str] = set()

    # Prefer the ESP-IDF command for any source file that exists in both
    # builds so IDE symbol resolution follows the target toolchain.
    for entry in esp_entries + host_entries:
        file_path = entry.get("file")
        if not isinstance(file_path, str):
            raise ValueError(
                "compile_commands entries must include a string 'file' field"
            )
        if file_path in seen_files:
            continue
        seen_files.add(file_path)
        merged.append(entry)

    return merged


def write_compile_commands(path: pathlib.Path, entries: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        json.dump(entries, handle, indent=2)
        handle.write("\n")


def main(argv: list[str] | None = None) -> int:
    _ = argv

    try:
        build_host_project()
        build_esp32_project()
        host_entries = load_compile_commands(host_build_dir() / "compile_commands.json")
        esp_entries = load_compile_commands(esp_build_dir() / "compile_commands.json")
        merged_entries = merge_compile_commands(host_entries, esp_entries)
        write_compile_commands(output_file(), merged_entries)
    except (FileNotFoundError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"generate_compile_commands.py: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
