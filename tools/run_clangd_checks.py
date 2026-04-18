#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from queue import Empty, Queue
from threading import Thread

from pylsp_jsonrpc.endpoint import Endpoint
from pylsp_jsonrpc.streams import JsonRpcStreamReader, JsonRpcStreamWriter

REPO_ROOT = Path(__file__).resolve().parents[1]
CLANGD_CONFIG = REPO_ROOT / ".clangd"
SETTINGS_FILE = REPO_ROOT / ".vscode" / "settings.json"
TIMEOUT_SECONDS = 15.0
IDLE_SECONDS = 1.0


def load_settings_arguments() -> list[str]:
    if not SETTINGS_FILE.is_file():
        return []
    settings = json.loads(SETTINGS_FILE.read_text(encoding="utf-8"))
    arguments = settings.get("clangd.arguments", [])
    if not isinstance(arguments, list) or any(
        not isinstance(arg, str) for arg in arguments
    ):
        raise SystemExit(
            f"[clangd-check] ERROR: {SETTINGS_FILE} must contain a string list at `clangd.arguments`."
        )
    return list(arguments)


def clangd_binary() -> str:
    clangd = shutil.which("clangd")
    if clangd is None:
        raise SystemExit("[clangd-check] ERROR: `clangd` is not on PATH.")
    return clangd


def path_match() -> re.Pattern[str] | None:
    if not CLANGD_CONFIG.is_file():
        return None
    for line in CLANGD_CONFIG.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("PathMatch:"):
            return re.compile(line.split(":", 1)[1].strip())
    return None


def repo_files(pattern: re.Pattern[str] | None) -> list[Path]:
    files = []
    for path in sorted(REPO_ROOT.rglob("*")):
        if path.is_file():
            rel = path.relative_to(REPO_ROOT).as_posix()
            if pattern is None or pattern.match(rel):
                files.append(path)
    return files


def language_id(path: Path) -> str:
    return "c" if path.suffix.lower() == ".c" else "cpp"


def diagnostic_lines(payload: dict[str, object]) -> list[tuple[str, str]]:
    uri = str(payload.get("uri", ""))
    raw = payload.get("diagnostics", [])
    if not isinstance(raw, list):
        return []

    lines: list[tuple[str, str]] = []
    for item in raw:
        if not isinstance(item, dict):
            continue
        severity = item.get("severity")
        if severity not in {1, 2}:
            continue
        rng = item.get("range", {})
        start = rng.get("start", {}) if isinstance(rng, dict) else {}
        message = item.get("message")
        if not isinstance(start, dict) or not isinstance(message, str):
            continue
        code = item.get("code")
        suffix = (
            f" [{code}]"
            if isinstance(code, str)
            else f" [{code}]" if code is not None else ""
        )
        kind = "error" if severity == 1 else "warning"
        line = int(start.get("line", 0)) + 1
        col = int(start.get("character", 0)) + 1
        lines.append((uri, f"{uri}:{line}:{col}: {kind}: {message}{suffix}"))
    return lines


def main() -> int:
    pattern = path_match()
    files = repo_files(pattern)
    clangd = clangd_binary()
    file_uris = {path.resolve().as_uri() for path in files}

    proc = subprocess.Popen(
        [clangd, "--enable-config", "--log=error", *load_settings_arguments()],
        cwd=REPO_ROOT,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )

    queue: Queue[dict[str, object]] = Queue()
    dispatcher = {"textDocument/publishDiagnostics": queue.put}
    endpoint = Endpoint(
        dispatcher, JsonRpcStreamWriter(proc.stdin, ensure_ascii=False).write
    )
    reader = JsonRpcStreamReader(proc.stdout)
    Thread(target=reader.listen, args=(endpoint.consume,), daemon=True).start()

    try:
        initialize = endpoint.request(
            "initialize",
            {
                "processId": os.getpid(),
                "rootUri": REPO_ROOT.as_uri(),
                "capabilities": {},
                "workspaceFolders": [
                    {"uri": REPO_ROOT.as_uri(), "name": REPO_ROOT.name}
                ],
            },
        )
        initialize.result(timeout=TIMEOUT_SECONDS)
        endpoint.notify("initialized", {})

        for path in files:
            endpoint.notify(
                "textDocument/didOpen",
                {
                    "textDocument": {
                        "uri": path.resolve().as_uri(),
                        "languageId": language_id(path),
                        "version": 1,
                        "text": path.read_text(encoding="utf-8"),
                    }
                },
            )

        deadline = time.monotonic() + TIMEOUT_SECONDS
        idle_deadline = time.monotonic() + IDLE_SECONDS
        payloads: dict[str, dict[str, object]] = {}
        while True:
            remaining = min(
                deadline - time.monotonic(), idle_deadline - time.monotonic()
            )
            if remaining <= 0:
                break
            try:
                payload = queue.get(timeout=remaining)
            except Empty:
                break
            if not isinstance(payload, dict):
                continue
            uri = str(payload.get("uri", ""))
            if uri in file_uris:
                payloads[uri] = payload
                idle_deadline = time.monotonic() + IDLE_SECONDS

        lines = sorted(
            {
                line
                for payload in payloads.values()
                for line in diagnostic_lines(payload)
            },
            key=lambda item: item[0] + item[1],
        )
        if not lines:
            print(
                f"[clangd-check] No clang-tidy diagnostics found in {len(files)} files using {clangd}"
            )
            return 0

        for _, line in lines:
            print(f"[clangd-check] {line}")
        warnings = sum(
            1
            for payload in payloads.values()
            for diag in payload.get("diagnostics", [])
            if isinstance(diag, dict) and diag.get("severity") == 2
        )
        errors = sum(
            1
            for payload in payloads.values()
            for diag in payload.get("diagnostics", [])
            if isinstance(diag, dict) and diag.get("severity") == 1
        )
        print(
            f"[clangd-check] Found {warnings} warning(s) and {errors} error(s) using {clangd}",
            file=sys.stderr,
        )
        return 1
    finally:
        try:
            endpoint.request("shutdown", None).result(timeout=TIMEOUT_SECONDS)
        except Exception:
            pass
        try:
            endpoint.notify("exit", {})
        except Exception:
            pass
        if proc.stdin is not None:
            proc.stdin.close()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    raise SystemExit(main())
