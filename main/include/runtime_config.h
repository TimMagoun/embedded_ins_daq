#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board_profile.h"
#include "esp_compat.h"
#include "runtime_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Names the reason runtime configuration validation failed. */
typedef enum {
  RUNTIME_CONFIG_ERROR_NONE = 0,
  RUNTIME_CONFIG_ERROR_NULL_CONFIG,
  RUNTIME_CONFIG_ERROR_PORT_COUNT_INVALID,
  RUNTIME_CONFIG_ERROR_NO_ENABLED_PORTS,
  RUNTIME_CONFIG_ERROR_UART_PORT_INVALID,
  RUNTIME_CONFIG_ERROR_BAUD_RATE_INVALID,
  RUNTIME_CONFIG_ERROR_TIMING_MODE_INVALID,
  RUNTIME_CONFIG_ERROR_SYNC_EDGE_MODE_INVALID,
  RUNTIME_CONFIG_ERROR_TIMING_MODE_CONFLICT,
  RUNTIME_CONFIG_ERROR_TRIGGER_PULSE_WIDTH_INVALID,
} runtime_config_error_t;

/* Captures the runtime behavior selected for one logical data port. */
typedef struct {
  bool enabled;
  int uart_port;
  int baud_rate;
  port_timing_mode_t timing_mode;
  sync_edge_mode_t sync_edge_mode;
  bool enable_sync_input;
  uint32_t trigger_period_us;
  uint32_t trigger_pulse_width_us;
} runtime_port_config_t;

/* Carries the validated runtime session contract used at boot. */
typedef struct {
  size_t port_count;
  runtime_port_config_t ports[BOARD_PORT_COUNT];
} runtime_config_t;

/* Describes operator-selected settings before board capability mapping. */
typedef struct {
  bool enabled;
  int baud_rate;
  port_timing_mode_t timing_mode;
  sync_edge_mode_t sync_edge_mode;
  bool enable_sync_input;
  uint32_t trigger_period_us;
  uint32_t trigger_pulse_width_us;
} runtime_config_source_port_t;

/* Holds the operator-selected runtime configuration source data. */
typedef struct {
  size_t port_count;
  runtime_config_source_port_t ports[BOARD_PORT_COUNT];
} runtime_config_source_t;

/* Validates the runtime session contract before startup proceeds. */
esp_err_t runtime_config_validate(const runtime_config_t* config,
                                  runtime_config_error_t* error);

/* Returns a short message for the given runtime validation error. */
const char* runtime_config_error_message(runtime_config_error_t error);

#ifdef __cplusplus
}
#endif
