#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "binary_log_pipeline.h"
#include "board_profile.h"
#include "clock_probe.h"
#include "clock_service.h"
#include "clock_smoke.h"
#include "esp_err.h"
#include "esp_log.h"
#include "ethernet_smoke.h"
#include "fault_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform_config_adapter.h"
#include "platform_iram.h"
#include "record_builder.h"
#include "runtime_banner.h"
#include "runtime_config.h"
#include "session_controller.h"
#include "status_log_pipeline.h"
#include "storage_service.h"
#include "uart_capture_service.h"
#include "uart_hal_adapter.h"


static const char* TAG = "embedded_ins_daq";
static const uint32_t kClockSmokeTaskDelayMs = 50U;
static const uint32_t kClockSmokeMaxAttempts = 20U;
static const uint32_t kSessionId = 1U;

#define UART_CAPTURE_READ_TIMEOUT_MS 50U
#define UART_CAPTURE_OVERALL_TIMEOUT_US 8000000ULL
#define UART_CAPTURE_IDLE_TIMEOUT_US 250000ULL
#define UART_CAPTURE_RETENTION_MS 500U
#define UART_CAPTURE_READ_BUFFER_BYTES 128U
#define UART_CAPTURE_RING_BYTES \
  (((921600U * UART_CAPTURE_RETENTION_MS) + 9999U) / 10000U)
#define SESSION_PIPELINE_BYTES 4096U
#define STATUS_PIPELINE_BYTES 512U

static PLATFORM_INTERNAL_RAM clock_smoke_isr_state_t s_isr_smoke_state;
static PLATFORM_INTERNAL_RAM uint8_t
    s_uart_capture_rings[BOARD_PORT_COUNT][UART_CAPTURE_RING_BYTES];
static PLATFORM_INTERNAL_RAM uint8_t s_session_pipeline[SESSION_PIPELINE_BYTES];
static char s_status_pipeline[STATUS_PIPELINE_BYTES];
static storage_service_t s_storage_service;

typedef struct {
  port_id_t port_id;
  size_t board_index;
  board_port_profile_t active_uart_port;
  const runtime_port_config_t* runtime_port;
  uart_capture_service_t capture_service;
  uart_hal_adapter_t uart_adapter;
  uint64_t last_rx_timestamp_us;
  size_t total_bytes;
  bool complete;
} reference_capture_port_t;

static reference_capture_port_t s_reference_ports[BOARD_PORT_COUNT];
static binary_log_pipeline_t s_binary_pipeline;
static status_log_pipeline_t s_status_pipeline_service;
static record_buffer_t s_session_start_record;
static session_info_t s_active_session_info;

static const char* select_capture_case_name(const runtime_config_t* config) {
  size_t enabled_ports = 0U;

  if (config == NULL) {
    return "platform_smoke";
  }

  for (size_t i = 0; i < config->port_count; ++i) {
    if (config->ports[i].enabled) {
      enabled_ports += 1U;
    }
  }

  if (enabled_ports >= 2U) {
    return "two_port_reference_capture";
  }
  return "port1_sd_logger";
}

static esp_err_t append_fault_record(binary_log_pipeline_t* pipeline,
                                     port_id_t port_id,
                                     const fault_event_t* event,
                                     health_status_t health) {
  record_buffer_t record = {};

  if (pipeline == NULL || event == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = record_builder_build_fault_event(
      clock_now_us(), (uint32_t)port_id, event, health, &record);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to encode fault record: %s", esp_err_to_name(err));
    return err;
  }

  err = binary_log_pipeline_append(pipeline, &record, NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to append fault record: %s", esp_err_to_name(err));
    return err;
  }

  return ESP_OK;
}

static void emit_session_artifact(binary_log_pipeline_t* pipeline) {
  static const char kHexDigits[] = "0123456789abcdef";
  uint8_t flush_buffer[64] = {0};
  char hex_line[(sizeof(flush_buffer) * 2U) + 1U];
  size_t bytes_written = 0U;

  if (pipeline == NULL) {
    return;
  }

  while (binary_log_pipeline_pending_bytes(pipeline) > 0U) {
    if (binary_log_pipeline_flush(pipeline, flush_buffer, sizeof(flush_buffer),
                                  &bytes_written) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to flush session pipeline for artifact output");
      return;
    }

    for (size_t i = 0; i < bytes_written; ++i) {
      hex_line[i * 2U] = kHexDigits[(flush_buffer[i] >> 4U) & 0x0fU];
      hex_line[i * 2U + 1U] = kHexDigits[flush_buffer[i] & 0x0fU];
    }
    hex_line[bytes_written * 2U] = '\0';
    ESP_LOGI(TAG, "SESSION_BIN_HEX %s", hex_line);
  }
}

static void emit_file_artifact_hex(const char* artifact_name,
                                   const char* path_on_disk) {
  static const char kHexDigits[] = "0123456789abcdef";
  uint8_t read_buffer[64] = {0};
  char hex_line[(sizeof(read_buffer) * 2U) + 1U];
  FILE* file = NULL;

  if (artifact_name == NULL || path_on_disk == NULL) {
    return;
  }

  file = fopen(path_on_disk, "rb");
  if (file == NULL) {
    ESP_LOGW(TAG, "Unable to open artifact file for emission: %s",
             path_on_disk);
    return;
  }

  for (;;) {
    const size_t bytes_read = fread(read_buffer, 1U, sizeof(read_buffer), file);
    if (bytes_read == 0U) {
      break;
    }

    for (size_t i = 0; i < bytes_read; ++i) {
      hex_line[i * 2U] = kHexDigits[(read_buffer[i] >> 4U) & 0x0fU];
      hex_line[i * 2U + 1U] = kHexDigits[read_buffer[i] & 0x0fU];
    }
    hex_line[bytes_read * 2U] = '\0';
    ESP_LOGI(TAG, "ARTIFACT_HEX %s %s", artifact_name, hex_line);
  }

  fclose(file);
}

static esp_err_t drain_binary_pipeline_to_storage(
    storage_service_t* storage, binary_log_pipeline_t* pipeline) {
  uint8_t flush_buffer[128] = {0};
  size_t bytes_written = 0U;

  if (storage == NULL || pipeline == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  while (binary_log_pipeline_pending_bytes(pipeline) > 0U) {
    ESP_ERROR_CHECK(binary_log_pipeline_flush(
        pipeline, flush_buffer, sizeof(flush_buffer), &bytes_written));
    if (bytes_written == 0U) {
      break;
    }

    const esp_err_t err = storage_service_write_binary_block(
        storage, flush_buffer, bytes_written);
    if (err != ESP_OK) {
      return err;
    }
  }

  return ESP_OK;
}

static esp_err_t drain_status_pipeline_to_storage(
    storage_service_t* storage, status_log_pipeline_t* pipeline) {
  char flush_buffer[128] = {0};
  size_t bytes_written = 0U;

  if (storage == NULL || pipeline == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  while (status_log_pipeline_pending_bytes(pipeline) > 0U) {
    ESP_ERROR_CHECK(status_log_pipeline_flush(
        pipeline, flush_buffer, sizeof(flush_buffer) - 1U, &bytes_written));
    if (bytes_written == 0U) {
      break;
    }

    flush_buffer[bytes_written] = '\0';
    const char* cursor = flush_buffer;
    while (*cursor != '\0') {
      const char* newline = strchr(cursor, '\n');
      if (newline == NULL) {
        if (*cursor != '\0') {
          const esp_err_t err =
              storage_service_write_status_message(storage, cursor);
          if (err != ESP_OK) {
            return err;
          }
        }
        break;
      }

      char line[128] = {0};
      const size_t line_length = (size_t)(newline - cursor);
      memcpy(line, cursor, line_length);
      line[line_length] = '\0';
      if (storage_service_write_status_message(storage, line) != ESP_OK) {
        return ESP_FAIL;
      }
      cursor = newline + 1;
    }
  }

  return ESP_OK;
}

static bool drain_capture_backpressure(uart_capture_service_t* capture_service,
                                       binary_log_pipeline_t* pipeline) {
  fault_event_t ignored_fault = {};

  if (capture_service == NULL || pipeline == NULL) {
    return false;
  }

  if (!uart_capture_service_take_pending_fault(capture_service,
                                               &ignored_fault) ||
      ignored_fault.code != FAULT_CODE_STORAGE_BACKPRESSURE) {
    return false;
  }

  /* The caller decides how to drain the staging pipeline after acknowledging
   * recoverable storage backpressure. */
  (void)pipeline;
  return true;
}

static esp_err_t flush_port_capture(reference_capture_port_t* port,
                                    binary_log_pipeline_t* pipeline,
                                    storage_service_t* storage) {
  esp_err_t status;

  if (port == NULL || pipeline == NULL || storage == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  status = uart_capture_service_publish_pending(&port->capture_service, NULL);
  if (status == ESP_ERR_NO_MEM &&
      drain_capture_backpressure(&port->capture_service, pipeline)) {
    status = drain_binary_pipeline_to_storage(storage, pipeline);
    if (status == ESP_OK) {
      status =
          uart_capture_service_publish_pending(&port->capture_service, NULL);
    }
  }

  return status;
}

static void deinit_reference_ports(reference_capture_port_t* ports,
                                   size_t port_count) {
  if (ports == NULL) {
    return;
  }

  for (size_t i = 0; i < port_count; ++i) {
    uart_hal_adapter_deinit(&ports[i].uart_adapter);
  }
}

static void surface_capture_faults(reference_capture_port_t* ports,
                                   size_t port_count,
                                   session_controller_t* controller,
                                   binary_log_pipeline_t* pipeline) {
  if (ports == NULL || controller == NULL || pipeline == NULL) {
    return;
  }

  for (size_t i = 0; i < port_count; ++i) {
    fault_event_t fault = {};
    if (!uart_capture_service_take_pending_fault(&ports[i].capture_service,
                                                 &fault)) {
      continue;
    }

    session_controller_publish_fault(controller, &fault);
    (void)append_fault_record(pipeline, ports[i].port_id, &fault,
                              fault_manager_health(&controller->fault_manager));
  }
}

static runtime_config_source_t default_runtime_config_source(
    const board_profile_t* board) {
  runtime_config_source_t source = {0};

  source.port_count = BOARD_PORT_COUNT;
  for (size_t i = 0; i < BOARD_PORT_COUNT; ++i) {
    source.ports[i].enabled = board->ports[i].enabled;
    source.ports[i].baud_rate = (i < 2U) ? 9600 : 921600;
    source.ports[i].timing_mode = PORT_TIMING_DISABLED;
  }
  source.ports[0].timing_mode = PORT_TIMING_SYNC_INPUT;
  source.ports[0].sync_edge_mode = SYNC_EDGE_RISING;
  source.ports[0].enable_sync_input = true;

  return source;
}

static uint64_t app_clock_probe_read(void* ctx) {
  (void)ctx;
  return clock_now_us();
}

static bool run_clock_monotonicity_smoke(void) {
  uint32_t attempt;
  uint64_t last_sample = 0;

  if (!clock_probe_monotonic(app_clock_probe_read, NULL, 64U, &last_sample)) {
    ESP_LOGE(TAG, "Clock monotonicity probe failed in task context");
    return false;
  }

  ESP_LOGI(TAG, "Clock monotonicity probe passed: last_sample_us=%llu",
           (unsigned long long)last_sample);

  for (attempt = 0; attempt < kClockSmokeMaxAttempts; ++attempt) {
    if (clock_smoke_isr_ready(&s_isr_smoke_state, 3U)) {
      ESP_LOGI(TAG, "Clock ISR smoke passed: isr_samples=%lu last_isr_us=%llu",
               (unsigned long)s_isr_smoke_state.isr_sample_count,
               (unsigned long long)s_isr_smoke_state.last_isr_timestamp_us);
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(kClockSmokeTaskDelayMs));
  }

  ESP_LOGE(TAG, "Clock ISR smoke timed out: isr_samples=%lu",
           (unsigned long)s_isr_smoke_state.isr_sample_count);
  return false;
}
// Add the prototype here to satisfy compiler check
void app_main(void);
void app_main(void) {
  const board_profile_t* board = board_profile_active();
  board_profile_validation_result_t validation = {0};
  runtime_config_source_t runtime_source = {0};
  runtime_config_t runtime_config = {0};
  session_controller_t session_controller = {0};

  ESP_ERROR_CHECK(clock_init());
  if (!board_profile_validate(board, &validation)) {
    ESP_LOGE(TAG, "Board profile validation failed: %s",
             board_profile_validation_message(validation.code));
    ESP_LOGE(TAG, "Failure details: port_index=%d gpio_a=%d gpio_b=%d",
             validation.port_index, validation.gpio_a, validation.gpio_b);
    abort();
  }

  runtime_source = default_runtime_config_source(board);
  ESP_ERROR_CHECK(platform_config_adapter_build_runtime(board, &runtime_source,
                                                        &runtime_config));
  session_controller_init(&session_controller);
  storage_service_init(&s_storage_service, board->sdmmc_slot);
  ESP_ERROR_CHECK(storage_service_mount(&s_storage_service));
  ESP_ERROR_CHECK(session_controller_mark_storage_ready(&session_controller));
  if (session_controller_mark_config_loaded(&session_controller,
                                            &runtime_config) != ESP_OK) {
    ESP_LOGE(TAG, "Runtime config invalid: %s",
             runtime_config_error_message(
                 session_controller_last_config_error(&session_controller)));
    abort();
  }

  runtime_banner_log_startup(board);
  ESP_ERROR_CHECK(clock_smoke_start_isr(&s_isr_smoke_state));

  if (run_clock_monotonicity_smoke()) {
    runtime_banner_log_ready("clock_monotonicity");
  } else {
    ESP_LOGE(TAG, "Clock monotonicity smoke failed");
  }

  if (ethernet_smoke_run() == ESP_OK) {
    runtime_banner_log_ready("ethernet_smoke");
  } else {
    ESP_LOGE(TAG, "Ethernet smoke failed");
  }

  ESP_ERROR_CHECK(session_controller_start_autonomously(&session_controller));
  storage_service_unmount(&s_storage_service);
  runtime_banner_start_health_task();
  runtime_banner_log_ready("platform_smoke");
}
