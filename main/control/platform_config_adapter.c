#include "platform_config_adapter.h"

#include <string.h>

esp_err_t platform_config_adapter_build_runtime(
    const board_profile_t* board, const runtime_config_source_t* source,
    runtime_config_t* out) {
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  if (board == NULL || source == NULL || out == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (source->port_count > BOARD_PORT_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(out, 0, sizeof(*out));
  out->port_count = BOARD_PORT_COUNT;

  for (size_t i = 0; i < BOARD_PORT_COUNT; ++i) {
    const bool in_source = i < source->port_count;
    const bool board_enabled = board->ports[i].enabled;
    runtime_port_config_t* port = &out->ports[i];

    port->enabled = in_source && board_enabled && source->ports[i].enabled;
    port->uart_port = board->ports[i].uart_controller;
    port->baud_rate = source->ports[i].baud_rate;
    port->timing_mode = source->ports[i].timing_mode;
    port->sync_edge_mode = source->ports[i].sync_edge_mode;
    port->enable_sync_input = source->ports[i].enable_sync_input;
    port->trigger_period_us = source->ports[i].trigger_period_us;
    port->trigger_pulse_width_us = source->ports[i].trigger_pulse_width_us;

    if (!board_enabled) {
      port->enabled = false;
      port->timing_mode = PORT_TIMING_DISABLED;
      port->enable_sync_input = false;
      port->sync_edge_mode = SYNC_EDGE_NONE;
      port->trigger_period_us = 0;
      port->trigger_pulse_width_us = 0;
    }
  }

  if (runtime_config_validate(out, &error) != ESP_OK) {
    return ESP_ERR_INVALID_ARG;
  }

  return ESP_OK;
}
