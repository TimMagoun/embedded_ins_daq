#pragma once

#include <stdbool.h>

#include "board_profile.h"
#include "runtime_types.h"

#if __has_include("esp_err.h")
#include "esp_err.h"
#else
#ifndef RUNTIME_ESP_ERR_COMPAT_DEFINED
#define RUNTIME_ESP_ERR_COMPAT_DEFINED
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#endif
#ifndef ESP_ERR_NOT_SUPPORTED
#define ESP_ERR_NOT_SUPPORTED 0x106
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Configures the selected port sync GPIO for input edge capture. */
esp_err_t sync_hal_adapter_configure_input(port_id_t port_id,
                                           sync_edge_mode_t edge_mode);

/* Removes sync-edge configuration from the selected port GPIO. */
void sync_hal_adapter_deinit_input(port_id_t port_id);

#ifdef __cplusplus
}
#endif
