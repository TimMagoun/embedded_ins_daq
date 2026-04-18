#!/usr/bin/env bash
setup_script="${BASH_SOURCE[0]-}"
if [[ -z "${setup_script}" && -n "${ZSH_VERSION-}" ]]; then
    setup_script="$(eval 'printf %s "${(%):-%x}"')"
fi

if [[ -z "${setup_script}" ]]; then
    printf 'tools/setup.sh: unable to determine script path\n' >&2
    return 1 2> /dev/null || exit 1
fi

script_dir="$(cd "$(dirname "${setup_script}")" && pwd)"
source "${script_dir}/../esp-idf/export.sh"
