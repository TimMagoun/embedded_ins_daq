#include "runtime_config.h"

static void set_error(runtime_config_error_t* error,
                      runtime_config_error_t value) {
  if (error != NULL) {
    *error = value;
  }
}

static bool port_timing_mode_valid(port_timing_mode_t mode) {
  return mode == PORT_TIMING_NONE || mode == PORT_TIMING_SYNC ||
         mode == PORT_TIMING_TRIGGER;
}

static bool sync_edge_mode_valid(sync_edge_mode_t mode) {
  return mode == SYNC_EDGE_RISING || mode == SYNC_EDGE_FALLING ||
         mode == SYNC_EDGE_CHANGE;
}

runtime_config_t runtime_config_default(void) {
  runtime_config_t config = {0};

  config.ports[0].enabled = true;
  config.ports[0].baud_rate = 115200;
  config.ports[0].timing_mode = PORT_TIMING_NONE;
  config.ports[0].sync_edge_mode = SYNC_EDGE_RISING;

  return config;
}

esp_err_t runtime_config_validate(const runtime_config_t* config,
                                  runtime_config_error_t* error) {
  bool any_enabled = false;

  if (config == NULL) {
    set_error(error, RUNTIME_CONFIG_ERROR_NULL_CONFIG);
    return ESP_ERR_INVALID_ARG;
  }

  for (size_t i = 0; i < BOARD_PORT_COUNT; ++i) {
    const runtime_port_config_t* port = &config->ports[i];
    if (!port->enabled) {
      continue;
    }

    any_enabled = true;

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

    if (port->timing_mode == PORT_TIMING_TRIGGER) {
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
    case RUNTIME_CONFIG_ERROR_NO_ENABLED_PORTS:
      return "runtime config must enable at least one port";
    case RUNTIME_CONFIG_ERROR_BAUD_RATE_INVALID:
      return "runtime config baud rate must be positive";
    case RUNTIME_CONFIG_ERROR_TIMING_MODE_INVALID:
      return "runtime config timing mode is invalid";
    case RUNTIME_CONFIG_ERROR_SYNC_EDGE_MODE_INVALID:
      return "runtime config sync edge mode is invalid";
    case RUNTIME_CONFIG_ERROR_TRIGGER_PULSE_WIDTH_INVALID:
      return "runtime trigger pulse width must be non-zero and less than its "
             "period";
    default:
      return "unknown runtime config validation result";
  }
}
