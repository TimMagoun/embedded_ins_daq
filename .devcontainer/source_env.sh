#!/bin/bash
# Source the ESP-IDF export environment automatically in interactive shells.
[[ $- != *i* ]] && return 0

if [[ -n "${EMBEDDED_INS_DAQ_IDF_LOADED:-}" ]]; then
    return 0
fi

if [[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/export.sh" ]]; then
    export EMBEDDED_INS_DAQ_IDF_LOADED=1
    source "${IDF_PATH}/export.sh"
fi
