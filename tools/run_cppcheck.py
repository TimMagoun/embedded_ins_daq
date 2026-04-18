from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def filter_entries(
    entries: list[dict[str, object]], project_dir: Path
) -> list[dict[str, object]]:
    project_path = project_dir.resolve()
    filtered: list[dict[str, object]] = []

    for entry in entries:
        entry_file = Path(str(entry.get("file", ""))).resolve()
        if entry_file.is_relative_to(project_path):
            filtered.append(entry)

    return filtered


def build_cppcheck_command(project_file: Path, strict: bool) -> list[str]:
    command = [
        "cppcheck",
        f"--project={project_file}",
        f"-j{os.cpu_count() or 1}",
        "--enable=warning,performance,portability",
        "--inline-suppr",
        "--force",
        "--quiet",
        "--error-exitcode=1",
        "--suppress=missingIncludeSystem",
        "--suppress=unmatchedSuppression",
        "-D__cplusplus=201703L",
    ]

    if strict:
        command.extend(
            [
                "--enable=style,information,missingInclude",
                "--inconclusive",
                "--check-level=exhaustive",
                "--suppress=checkersReport",
            ]
        )

    return command


def run(cmd: list[str], cwd: Path) -> None:
    completed = subprocess.run(cmd, cwd=cwd, check=False)
    if completed.returncode != 0:
        raise SystemExit(completed.returncode)


def build_firmware(repo_root: Path) -> None:
    setup_script = repo_root / "tools" / "setup.sh"
    command = ["bash", "-lc", f"source {setup_script} && idf.py build"]
    run(command, repo_root)


def load_filtered_project(repo_root: Path) -> list[dict[str, object]]:
    project_file = repo_root / "build" / "compile_commands.json"
    if not project_file.is_file():
        raise SystemExit(f"[cppcheck] ERROR: Missing required file: {project_file}")

    with project_file.open() as handle:
        entries = json.load(handle)

    return filter_entries(entries, repo_root / "main")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog="tools/run_cppcheck.py")
    parser.add_argument("--strict", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    _ = parse_args(argv)
    repo_root = Path(__file__).resolve().parents[1]

    if shutil.which("cppcheck") is None:
        raise SystemExit(
            "[cppcheck] ERROR: Required tool 'cppcheck' is missing from PATH."
        )

    strict = "--strict" in argv
    build_firmware(repo_root)
    filtered_entries = load_filtered_project(repo_root)

    with tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".json",
        prefix="cppcheck-main-",
        delete=False,
    ) as handle:
        json.dump(filtered_entries, handle)
        filtered_project = Path(handle.name)

    try:
        print("[cppcheck] Analyzing firmware sources under main/")
        run(build_cppcheck_command(filtered_project, strict), repo_root)
    finally:
        filtered_project.unlink(missing_ok=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
