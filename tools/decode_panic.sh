#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./tools/decode_panic.sh --elf <firmware.elf> --panic-log <monitor.log> [--output <report.txt>]
EOF
}

ELF_PATH=""
PANIC_LOG=""
OUTPUT_PATH=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --elf)
            ELF_PATH=$2
            shift 2
            ;;
        --panic-log)
            PANIC_LOG=$2
            shift 2
            ;;
        --output)
            OUTPUT_PATH=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown argument: %s\n' "$1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

[[ -n "${ELF_PATH}" ]] || { usage >&2; exit 1; }
[[ -n "${PANIC_LOG}" ]] || { usage >&2; exit 1; }
[[ -f "${ELF_PATH}" ]] || { printf 'Missing ELF: %s\n' "${ELF_PATH}" >&2; exit 1; }
[[ -f "${PANIC_LOG}" ]] || { printf 'Missing panic log: %s\n' "${PANIC_LOG}" >&2; exit 1; }

if [[ -z "${OUTPUT_PATH}" ]]; then
    OUTPUT_PATH="$(dirname "${PANIC_LOG}")/decoded_backtrace.txt"
fi

ADDR2LINE_BIN=${ADDR2LINE_BIN:-riscv32-esp-elf-addr2line}
command -v "${ADDR2LINE_BIN}" >/dev/null 2>&1 || {
    printf 'Missing tool: %s\n' "${ADDR2LINE_BIN}" >&2
    exit 1
}

mapfile -t ADDRESSES < <(grep -Eo '0x4[0-9a-fA-F]{7}' "${PANIC_LOG}" | awk '!seen[$0]++')

{
    printf 'ELF: %s\n' "${ELF_PATH}"
    printf 'Panic log: %s\n' "${PANIC_LOG}"
    printf '\n'
    if [[ ${#ADDRESSES[@]} -eq 0 ]]; then
        printf 'No backtrace-style addresses were found in the panic log.\n'
    else
        printf 'Decoded addresses:\n'
        "${ADDR2LINE_BIN}" -pfiaC -e "${ELF_PATH}" "${ADDRESSES[@]}"
    fi
} > "${OUTPUT_PATH}"

printf '%s\n' "${OUTPUT_PATH}"
