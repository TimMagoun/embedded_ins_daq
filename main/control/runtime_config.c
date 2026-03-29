#include "runtime_config.h"

static void set_error(runtime_config_error_t* error,
                      runtime_config_error_t value) {
  if (error != NULL) {
    *error = value;
  }
}

static bool port_timing_mode_valid(port_timing_mode_t mode) {
  return mode == PORT_TIMING_DISABLED || mode == PORT_TIMING_SYNC_INPUT ||
         mode == PORT_TIMING_TRIGGER_OUTPUT;
}

static bool sync_edge_mode_valid(sync_edge_mode_t mode) {
  return mode == SYNC_EDGE_NONE || mode == SYNC_EDGE_RISING ||
         mode == SYNC_EDGE_FALLING || mode == SYNC_EDGE_BOTH;
}

esp_err_t runtime_config_validate(const runtime_config_t* config,
                                  runtime_config_error_t* error) {
  bool any_enabled = false;

  if (config == NULL) {
    set_error(error, RUNTIME_CONFIG_ERROR_NULL_CONFIG);
    return ESP_ERR_INVALID_ARG;
  }

  if (config->port_count == 0 || config->port_count > BOARD_PORT_COUNT) {
    set_error(error, RUNTIME_CONFIG_ERROR_PORT_COUNT_INVALID);
    return ESP_ERR_INVALID_ARG;
  }

  for (size_t i = 0; i < config->port_count; ++i) {
    const runtime_port_config_t* port = &config->ports[i];
    if (!port->enabled) {
      continue;
    }

    any_enabled = true;

    if (port->uart_port == BOARD_UART_UNUSED) {
      set_error(error, RUNTIME_CONFIG_ERROR_UART_PORT_INVALID);
      return ESP_ERR_INVALID_ARG;
    }

    if (port->baud_rate <= 0) {
      set_error(error, RUNTIME_CONFIG_ERROR_BAUD_RATE_INVALID);
      return ESP_ERR_INVALID_ARG;
    }

    if (!port_timing_mode_valid(port->timing_mode)) {
      set_error(error, RUNTIME_CONFIG_ERROR_TIMING_MODE_INVALID);
      return ESP_ERR_INVALID_ARG;
    }

    if (!sync_edge_mode_valid(port->sync_edge_mode)) {
      set_error(error, RUNTIME_CONFIG_ERROR_SYNC_EDGE_MODE_INVALID);
      return ESP_ERR_INVALID_ARG;
    }

    if (port->timing_mode == PORT_TIMING_TRIGGER_OUTPUT &&
        port->enable_sync_input) {
      set_error(error, RUNTIME_CONFIG_ERROR_TIMING_MODE_CONFLICT);
      return ESP_ERR_INVALID_ARG;
    }

    if (port->timing_mode == PORT_TIMING_SYNC_INPUT &&
        port->sync_edge_mode == SYNC_EDGE_NONE) {
      set_error(error, RUNTIME_CONFIG_ERROR_TIMING_MODE_CONFLICT);
      return ESP_ERR_INVALID_ARG;
    }

    if (port->timing_mode == PORT_TIMING_TRIGGER_OUTPUT) {
      if (port->trigger_period_us == 0 || port->trigger_pulse_width_us == 0 ||
          port->trigger_pulse_width_us >= port->trigger_period_us) {
        set_error(error, RUNTIME_CONFIG_ERROR_TRIGGER_PULSE_WIDTH_INVALID);
        return ESP_ERR_INVALID_ARG;
      }
    }
  }

  if (!any_enabled) {
    set_error(error, RUNTIME_CONFIG_ERROR_NO_ENABLED_PORTS);
    return ESP_ERR_INVALID_ARG;
  }

  set_error(error, RUNTIME_CONFIG_ERROR_NONE);
  return ESP_OK;
}

const char* runtime_config_error_message(runtime_config_error_t error) {
  switch (error) {
    case RUNTIME_CONFIG_ERROR_NONE:
      return "runtime config is valid";
    case RUNTIME_CONFIG_ERROR_NULL_CONFIG:
      return "runtime config pointer is null";
    case RUNTIME_CONFIG_ERROR_PORT_COUNT_INVALID:
      return "runtime config port count is invalid";
    case RUNTIME_CONFIG_ERROR_NO_ENABLED_PORTS:
      return "runtime config must enable at least one port";
    case RUNTIME_CONFIG_ERROR_UART_PORT_INVALID:
      return "runtime config enabled port must map to a UART controller";
    case RUNTIME_CONFIG_ERROR_BAUD_RATE_INVALID:
      return "runtime config baud rate must be positive";
    case RUNTIME_CONFIG_ERROR_TIMING_MODE_INVALID:
      return "runtime config timing mode is invalid";
    case RUNTIME_CONFIG_ERROR_SYNC_EDGE_MODE_INVALID:
      return "runtime config sync edge mode is invalid";
    case RUNTIME_CONFIG_ERROR_TIMING_MODE_CONFLICT:
      return "runtime config port cannot enable sync input and trigger output "
             "together";
    case RUNTIME_CONFIG_ERROR_TRIGGER_PULSE_WIDTH_INVALID:
      return "runtime trigger pulse width must be less than its period";
    default:
      return "unknown runtime config validation result";
  }
}
