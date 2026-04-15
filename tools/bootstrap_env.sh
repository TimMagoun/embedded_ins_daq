#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
MIN_IDF_VERSION="5.3.0"
TARGET="esp32p4"

log() {
    printf '[bootstrap] %s\n' "$*"
}

fail() {
    printf '[bootstrap] ERROR: %s\n' "$*" >&2
    exit 1
}

version_ge() {
    local lhs
    lhs=$(printf '%s\n' "$1" "$2" | sort -V | head -n1)
    [[ "${lhs}" == "$2" ]]
}

require_tool() {
    local tool_name=$1
    command -v "${tool_name}" > /dev/null 2>&1 || fail "Required tool '${tool_name}' is missing from PATH."
}

[[ -n "${IDF_PATH:-}" ]] || fail "IDF_PATH is not set. Run 'source ./tools/setup.sh' first."

require_tool idf.py
require_tool cmake
require_tool ctest
require_tool uv
require_tool cppcheck

IDF_VERSION_RAW=$(idf.py --version)
IDF_VERSION=$(printf '%s\n' "${IDF_VERSION_RAW}" | sed -E 's/^ESP-IDF v//')

version_ge "${IDF_VERSION}" "${MIN_IDF_VERSION}" || fail "ESP-IDF ${IDF_VERSION} is too old. Require at least ${MIN_IDF_VERSION}."

idf.py --list-targets | grep -qx "${TARGET}" || fail "The active ESP-IDF does not support target '${TARGET}'."

mkdir -p "${REPO_ROOT}/artifacts/latest/device" "${REPO_ROOT}/artifacts/runs/device"

log "ESP-IDF version: ${IDF_VERSION}"
log "ESP-IDF path: ${IDF_PATH}"
log "Supported target '${TARGET}' detected."
log "uv: $(uv --version)"
log "cppcheck: $(cppcheck --version)"
log "Environment is assumed to be prepared by source ./tools/setup.sh."
log "Next steps:"
log "  1. idf.py set-target ${TARGET}   # once in a clean workspace"
log "  2. idf.py build"
log "  3. uv sync --group dev"
log "  4. uv run --group dev pre-commit install"
log "  5. uv run --group dev pre-commit run --all-files"
log "  6. cmake -S host_tests -B build_host && cmake --build build_host && ctest --test-dir build_host"
log "  7. idf.py -p <port> flash"
log "  8. python3 -m tools.monitor --ready-banner 'READY: platform_smoke'"
