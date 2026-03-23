#!/usr/bin/env bash

if [[ -n "${ZSH_EVAL_CONTEXT:-}" ]]; then
    case "${ZSH_EVAL_CONTEXT}" in
        *:file) _setup_is_sourced=1 ;;
        *) _setup_is_sourced=0 ;;
    esac
elif [[ -n "${BASH_VERSION:-}" ]]; then
    if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
        _setup_is_sourced=1
    else
        _setup_is_sourced=0
    fi
else
    _setup_is_sourced=0
fi

_setup_finish() {
    local code=$1
    unset -f _setup_finish
    unset _setup_is_sourced
    if [[ ${code} -eq 0 ]]; then
        return 0 2>/dev/null || exit 0
    fi
    return "${code}" 2>/dev/null || exit "${code}"
}

if [[ ${_setup_is_sourced} -ne 1 ]]; then
    printf '[setup] ERROR: source this script instead of executing it: source ./tools/setup.sh\n' >&2
    _setup_finish 1
fi

_setup_repo_root=$(pwd)
_setup_script_dir=""
if [[ ! -f "${_setup_repo_root}/esp.env" && -n "${BASH_SOURCE[0]:-}" ]]; then
    _setup_script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
    _setup_repo_root=$(cd "${_setup_script_dir}/.." && pwd)
fi
_setup_env_file="${_setup_repo_root}/esp.env"

if [[ ! -f "${_setup_env_file}" ]]; then
    printf '[setup] ERROR: missing %s. Copy esp.env.example to esp.env first.\n' "${_setup_env_file}" >&2
    _setup_finish 1
fi

set -a
# shellcheck source=/dev/null
source "${_setup_env_file}"
set +a

if [[ -z "${IDF_PATH:-}" ]]; then
    printf '[setup] ERROR: IDF_PATH is not set in %s.\n' "${_setup_env_file}" >&2
    _setup_finish 1
fi

if [[ ! -f "${IDF_PATH}/export.sh" ]]; then
    printf '[setup] ERROR: missing ESP-IDF export script: %s/export.sh\n' "${IDF_PATH}" >&2
    _setup_finish 1
fi

# shellcheck source=/dev/null
source "${IDF_PATH}/export.sh"

printf '[setup] Loaded esp.env and sourced %s/export.sh\n' "${IDF_PATH}"
printf '[setup] BOARD_PORT=%s\n' "${BOARD_PORT:-"(unset)"}"

unset _setup_script_dir
unset _setup_repo_root
unset _setup_env_file

_setup_finish 0
