#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
HOST_TEST_BUILD_DIR="${REPO_ROOT}/build_host"

fail() {
    printf '[cppcheck] ERROR: %s\n' "$*" >&2
    exit 1
}

require_tool() {
    local tool_name=$1
    command -v "${tool_name}" > /dev/null 2>&1 || fail "Required tool '${tool_name}' is missing from PATH."
}

require_tool cppcheck

STRICT=0
JOBS=$(nproc)

usage() {
    cat << 'EOF'
Usage: tools/run_cppcheck.sh [--strict]

  --strict   Enable additional cppcheck checks for slower, broader analysis.
EOF
}

while (($#)); do
    case "$1" in
        --strict)
            STRICT=1
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            fail "Unknown argument: $1"
            ;;
    esac
done

run_cppcheck() {
    local label=$1
    shift

    local -a cppcheck_args=(
        -j "${JOBS}"
        --enable=warning,performance,portability
        --inline-suppr
        --force
        --quiet
        --error-exitcode=1
        --suppress=missingIncludeSystem
        --suppress=unmatchedSuppression
    )

    if [[ "${STRICT}" -ne 0 ]]; then
        cppcheck_args+=(
            --enable=style,information,missingInclude
            --inconclusive
            --check-level=exhaustive
            --suppress=checkersReport
        )
    fi

    if [[ -n "${IDF_PATH:-}" ]]; then
        cppcheck_args+=(-i "${IDF_PATH}")
    fi

    printf '[cppcheck] Analyzing %s\n' "${label}"
    cppcheck "${cppcheck_args[@]}" "$@"
}

run_embedded_build() {
    local project_file=${REPO_ROOT}/build/compile_commands.json
    local project_dir=${REPO_ROOT}/main
    local tmp_file

    # shellcheck source=/dev/null
    source "${REPO_ROOT}/tools/setup.sh"
    (
        cd "${REPO_ROOT}" \
            && idf.py build
    )

    [[ -f "${project_file}" ]] || fail "Missing required file: ${project_file}. Run idf.py build first."

    tmp_file=$(mktemp "${TMPDIR:-/tmp}/cppcheck-embedded-build.XXXXXX.json")
    python3 - "${project_file}" "${project_dir}" "${tmp_file}" << 'PY'
import json
import pathlib
import sys

project_file = pathlib.Path(sys.argv[1])
project_dir = pathlib.Path(sys.argv[2])
tmp_file = pathlib.Path(sys.argv[3])

with project_file.open() as handle:
    entries = json.load(handle)

filtered = [
    entry
    for entry in entries
    if pathlib.Path(entry.get("file", "")).is_relative_to(project_dir)
]

with tmp_file.open("w") as handle:
    json.dump(filtered, handle)
PY

    trap 'rm -f "${tmp_file}"' RETURN

    run_cppcheck "embedded build sources" \
        --project="${tmp_file}" \
        --suppress="*:${IDF_PATH}/*"

    rm -f "${tmp_file}"
    trap - RETURN
}

collect_files() {
    local root=$1
    shift

    find "${root}" \
        -path "${root}/build" -prune -o \
        -path "${root}/build_*" -prune -o \
        -type f \( "$@" \) -print | sort
}

collect_host_test_includes() {
    local -a include_args=(
        -I "${REPO_ROOT}/main/include"
    )
    local -a candidate_dirs=(
        "${HOST_TEST_BUILD_DIR}/_deps/googletest-src/googletest/include"
        "${HOST_TEST_BUILD_DIR}/_deps/googletest-src/googlemock/include"
    )
    local candidate

    for candidate in "${candidate_dirs[@]}"; do
        if [[ -d "${candidate}" ]]; then
            include_args+=(-I "${candidate}")
        fi
    done

    printf '%s\n' "${include_args[@]}"
}

mapfile -t host_c_sources < <(
    collect_files "${REPO_ROOT}/host_tests" \
        -name '*.c' -o -name '*.h'
)

mapfile -t host_cpp_sources < <(
    collect_files "${REPO_ROOT}/host_tests" \
        -name '*.cc' -o -name '*.cpp' -o -name '*.hpp'
)
mapfile -t host_test_include_args < <(collect_host_test_includes)

run_embedded_build

run_cppcheck "host test C sources" \
    --std=c11 \
    --library=googletest \
    --suppress="*:${HOST_TEST_BUILD_DIR}/_deps/*" \
    "${host_test_include_args[@]}" \
    "${host_c_sources[@]}"

run_cppcheck "host test C++ sources" \
    --std=c++17 \
    --library=googletest \
    --suppress="*:${HOST_TEST_BUILD_DIR}/_deps/*" \
    "${host_test_include_args[@]}" \
    "${host_cpp_sources[@]}"
