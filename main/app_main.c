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
#include "sync_capture_service.h"
#include "sync_hal_adapter.h"
#include "uart_capture_service.h"
#include "uart_hal_adapter.h"

#if __has_include("driver/gpio.h")
#include "driver/gpio.h"
#define APP_MAIN_HAS_GPIO_DRIVER 1
#else
#define APP_MAIN_HAS_GPIO_DRIVER 0
#endif

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
#define SYNC_EVENT_QUEUE_CAPACITY 64U

static PLATFORM_INTERNAL_RAM clock_smoke_isr_state_t s_isr_smoke_state;
static PLATFORM_INTERNAL_RAM uint8_t
    s_uart_capture_rings[BOARD_PORT_COUNT][UART_CAPTURE_RING_BYTES];
static PLATFORM_INTERNAL_RAM uint8_t s_session_pipeline[SESSION_PIPELINE_BYTES];
static PLATFORM_INTERNAL_RAM sync_edge_event_t
    s_sync_event_queue[SYNC_EVENT_QUEUE_CAPACITY];
static char s_status_pipeline[STATUS_PIPELINE_BYTES];
static storage_service_t s_storage_service;

typedef struct {
  port_id_t port_id;
  int gpio_num;
  bool installed;
} sync_isr_context_t;

static PLATFORM_INTERNAL_RAM sync_isr_context_t
    s_sync_contexts[BOARD_PORT_COUNT];
static sync_capture_service_t s_sync_capture_service;
static bool s_sync_isr_service_installed;

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

static const char* select_capture_case_name(const runtime_config_t* config,
                                            size_t sync_events_captured,
                                            size_t fault_events_seen) {
  size_t enabled_ports = 0U;

  if (config == NULL) {
    return "platform_smoke";
  }

  for (size_t i = 0; i < config->port_count; ++i) {
    if (config->ports[i].enabled) {
      enabled_ports += 1U;
    }
  }

  if (enabled_ports >= 4U && fault_events_seen > 0U) {
    return "four_port_overload_faults";
  }
  if (sync_events_captured > 0U) {
    return "sync_input_capture";
  }
  if (enabled_ports >= 4U) {
    return "four_port_stress";
  }
  if (enabled_ports >= 2U) {
    return "two_port_reference_capture";
  }
  return "port1_sd_logger";
}

#if APP_MAIN_HAS_GPIO_DRIVER
static void PLATFORM_ISR_ATTR sync_input_isr(void* arg) {
  const sync_isr_context_t* context = (const sync_isr_context_t*)arg;
  const int level = gpio_get_level((gpio_num_t)context->gpio_num);
  (void)sync_capture_service_publish_isr(
      &s_sync_capture_service, context->port_id, clock_now_isr(), level != 0);
}
#endif

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

static esp_err_t drain_sync_capture(sync_capture_service_t* service,
                                    binary_log_pipeline_t* pipeline,
                                    storage_service_t* storage,
                                    session_controller_t* controller) {
  esp_err_t status = ESP_OK;

  if (service == NULL || pipeline == NULL || storage == NULL ||
      controller == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  status =
      sync_capture_service_drain(service, pipeline, &controller->fault_manager);
  if (status == ESP_ERR_NO_MEM) {
    status = drain_binary_pipeline_to_storage(storage, pipeline);
    if (status == ESP_OK) {
      status = sync_capture_service_drain(service, pipeline,
                                          &controller->fault_manager);
    }
  }

  if (fault_manager_has_fatal_fault(&controller->fault_manager)) {
    controller->state = SESSION_FAULTED;
  }

  return status;
}

static esp_err_t configure_sync_capture(const board_profile_t* board,
                                        const runtime_config_t* config) {
  size_t sync_port_count = 0U;

  if (board == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(s_sync_contexts, 0, sizeof(s_sync_contexts));
  ESP_ERROR_CHECK(sync_capture_service_init(
      &s_sync_capture_service, s_sync_event_queue, SYNC_EVENT_QUEUE_CAPACITY));

#if APP_MAIN_HAS_GPIO_DRIVER
  for (size_t i = 0; i < config->port_count; ++i) {
    const runtime_port_config_t* port = &config->ports[i];

    if (!port->enabled || !board->ports[i].enabled ||
        port->timing_mode != PORT_TIMING_SYNC_INPUT ||
        port->sync_edge_mode == SYNC_EDGE_NONE) {
      continue;
    }

    if (!s_sync_isr_service_installed) {
      esp_err_t isr_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
      if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        return isr_err;
      }
      s_sync_isr_service_installed = true;
    }

    ESP_ERROR_CHECK(sync_hal_adapter_configure_input((port_id_t)(i + 1U),
                                                     port->sync_edge_mode));
    s_sync_contexts[sync_port_count].port_id = (port_id_t)(i + 1U);
    s_sync_contexts[sync_port_count].gpio_num = board->ports[i].sync_gpio;
    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)board->ports[i].sync_gpio,
                                         sync_input_isr,
                                         &s_sync_contexts[sync_port_count]));
    s_sync_contexts[sync_port_count].installed = true;
    sync_port_count += 1U;
  }
#else
  for (size_t i = 0; i < config->port_count; ++i) {
    if (config->ports[i].enabled &&
        config->ports[i].timing_mode == PORT_TIMING_SYNC_INPUT) {
      return ESP_ERR_NOT_SUPPORTED;
    }
  }
#endif

  return ESP_OK;
}

static void deinit_sync_capture(void) {
#if APP_MAIN_HAS_GPIO_DRIVER
  for (size_t i = 0; i < BOARD_PORT_COUNT; ++i) {
    if (!s_sync_contexts[i].installed) {
      continue;
    }

    (void)gpio_isr_handler_remove((gpio_num_t)s_sync_contexts[i].gpio_num);
    sync_hal_adapter_deinit_input(s_sync_contexts[i].port_id);
    s_sync_contexts[i].installed = false;
  }
#endif
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

static esp_err_t run_reference_sd_logger_case(const board_profile_t* board,
                                              const runtime_config_t* config,
                                              session_controller_t* controller,
                                              storage_service_t* storage) {
  session_info_t session = {
      .session_id = kSessionId,
      .start_timestamp_us = clock_now_us(),
  };
  uint64_t deadline_us = clock_now_us() + UART_CAPTURE_OVERALL_TIMEOUT_US;
  size_t active_port_count = 0U;
  esp_err_t status = ESP_OK;

  if (board == NULL || config == NULL || controller == NULL ||
      storage == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(s_reference_ports, 0, sizeof(s_reference_ports));
  memset(&s_session_start_record, 0, sizeof(s_session_start_record));
  s_active_session_info = session;
  binary_log_pipeline_init(&s_binary_pipeline, s_session_pipeline,
                           sizeof(s_session_pipeline));
  status_log_pipeline_init(&s_status_pipeline_service, s_status_pipeline,
                           sizeof(s_status_pipeline));
  status = record_builder_build_session_start(&s_active_session_info, config,
                                              &s_session_start_record);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to encode session-start record: %s",
             esp_err_to_name(status));
    return status;
  }

  status = binary_log_pipeline_append(&s_binary_pipeline,
                                      &s_session_start_record, NULL);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to append session-start record: %s",
             esp_err_to_name(status));
    return status;
  }
  status = storage_service_open_session(storage, &s_active_session_info);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open SD-backed session: %s",
             esp_err_to_name(status));
    return status;
  }
  status = status_log_pipeline_append(&s_status_pipeline_service,
                                      "boot_autostart=1");
  if (status != ESP_OK) {
    return status;
  }
  status = storage_service_copy_config_snapshot(storage, config);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to persist config snapshot: %s",
             esp_err_to_name(status));
    return status;
  }
  status = drain_binary_pipeline_to_storage(storage, &s_binary_pipeline);
  if (status != ESP_OK) {
    return status;
  }
  status =
      drain_status_pipeline_to_storage(storage, &s_status_pipeline_service);
  if (status != ESP_OK) {
    return status;
  }

  status = configure_sync_capture(board, config);
  if (status != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure sync capture: %s",
             esp_err_to_name(status));
    return status;
  }

  for (size_t i = 0; i < config->port_count; ++i) {
    reference_capture_port_t* port;

    if (!config->ports[i].enabled || !board->ports[i].enabled) {
      continue;
    }

    port = &s_reference_ports[active_port_count];
    port->port_id = (port_id_t)(i + 1U);
    port->board_index = i;
    port->active_uart_port = board->ports[i];
    port->runtime_port = &config->ports[i];

    status = uart_capture_service_init(&port->capture_service, port->port_id,
                                       s_uart_capture_rings[active_port_count],
                                       sizeof(s_uart_capture_rings[0]),
                                       &s_binary_pipeline);
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize capture service for %s: %s",
               board->ports[i].name, esp_err_to_name(status));
      return status;
    }

    status = uart_hal_adapter_init(&port->uart_adapter, &port->active_uart_port,
                                   port->runtime_port->baud_rate,
                                   UART_CAPTURE_READ_BUFFER_BYTES * 2U);
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize %s UART adapter: %s",
               board->ports[i].name, esp_err_to_name(status));
      deinit_reference_ports(s_reference_ports, active_port_count);
      return status;
    }

    ESP_LOGI(TAG, "%s raw capture armed on UART%d TX=%d RX=%d at %d baud",
             board->ports[i].name, port->active_uart_port.uart_controller,
             port->active_uart_port.tx_gpio, port->active_uart_port.rx_gpio,
             port->runtime_port->baud_rate);
    ESP_LOGI(
        TAG, "%s retention=%u bytes for %u ms window", board->ports[i].name,
        (unsigned)uart_capture_required_retention_bytes(
            (uint32_t)port->runtime_port->baud_rate, UART_CAPTURE_RETENTION_MS),
        (unsigned)UART_CAPTURE_RETENTION_MS);
    active_port_count += 1U;
  }

  if (active_port_count == 0U) {
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Awaiting UART input on %u reference ports",
           (unsigned)active_port_count);

  while (clock_now_us() < deadline_us) {
    size_t complete_ports = 0U;

    for (size_t i = 0; i < active_port_count; ++i) {
      reference_capture_port_t* port = &s_reference_ports[i];
      uint8_t read_buffer[UART_CAPTURE_READ_BUFFER_BYTES] = {0};
      size_t bytes_read = 0U;

      if (port->complete) {
        complete_ports += 1U;
        continue;
      }

      status = uart_hal_adapter_read(&port->uart_adapter, read_buffer,
                                     sizeof(read_buffer),
                                     UART_CAPTURE_READ_TIMEOUT_MS, &bytes_read);
      if (status != ESP_OK) {
        break;
      }

      if (bytes_read > 0U) {
        const uint64_t rx_timestamp_us = clock_now_us();
        status = uart_capture_service_on_rx_bytes(
            &port->capture_service, port->port_id, rx_timestamp_us, read_buffer,
            bytes_read);
        if (status == ESP_ERR_NO_MEM &&
            drain_capture_backpressure(&port->capture_service,
                                       &s_binary_pipeline)) {
          status =
              drain_binary_pipeline_to_storage(storage, &s_binary_pipeline);
          if (status == ESP_OK) {
            status = uart_capture_service_on_rx_bytes(
                &port->capture_service, port->port_id, rx_timestamp_us,
                read_buffer, bytes_read);
          }
        }
        if (status != ESP_OK) {
          break;
        }
        port->total_bytes += bytes_read;
        port->last_rx_timestamp_us = rx_timestamp_us;
        continue;
      }

      if (port->total_bytes > 0U && port->last_rx_timestamp_us > 0U &&
          clock_now_us() - port->last_rx_timestamp_us >=
              UART_CAPTURE_IDLE_TIMEOUT_US) {
        status = flush_port_capture(port, &s_binary_pipeline, storage);
        if (status != ESP_OK) {
          break;
        }
        port->complete = true;
        complete_ports += 1U;
        continue;
      }
    }

    if (status != ESP_OK || complete_ports == active_port_count) {
      break;
    }

    status = drain_sync_capture(&s_sync_capture_service, &s_binary_pipeline,
                                storage, controller);
    if (status != ESP_OK) {
      break;
    }
  }

  for (size_t i = 0; i < active_port_count && status == ESP_OK; ++i) {
    if (s_reference_ports[i].complete) {
      continue;
    }

    status =
        flush_port_capture(&s_reference_ports[i], &s_binary_pipeline, storage);
    s_reference_ports[i].complete = (status == ESP_OK);
  }

  if (status != ESP_OK) {
    surface_capture_faults(s_reference_ports, active_port_count, controller,
                           &s_binary_pipeline);
    deinit_reference_ports(s_reference_ports, active_port_count);
    deinit_sync_capture();
    if (drain_binary_pipeline_to_storage(storage, &s_binary_pipeline) !=
        ESP_OK) {
      emit_session_artifact(&s_binary_pipeline);
    }
    storage_service_close_session(storage);
    return status;
  }

  deinit_reference_ports(s_reference_ports, active_port_count);
  deinit_sync_capture();

  for (size_t i = 0; i < active_port_count; ++i) {
    if (s_reference_ports[i].total_bytes > 0U) {
      continue;
    }

    ESP_LOGE(TAG, "%s raw capture timed out before any UART bytes arrived",
             board->ports[s_reference_ports[i].board_index].name);
    (void)drain_binary_pipeline_to_storage(storage, &s_binary_pipeline);
    storage_service_close_session(storage);
    return ESP_ERR_TIMEOUT;
  }

  status = status_log_pipeline_append(&s_status_pipeline_service,
                                      "capture_complete=1");
  if (status == ESP_OK) {
    status =
        drain_status_pipeline_to_storage(storage, &s_status_pipeline_service);
  }
  if (status == ESP_OK) {
    status = drain_sync_capture(&s_sync_capture_service, &s_binary_pipeline,
                                storage, controller);
  }
  if (status == ESP_OK) {
    status = drain_binary_pipeline_to_storage(storage, &s_binary_pipeline);
  }
  if (status != ESP_OK) {
    storage_service_close_session(storage);
    return status;
  }

  storage_service_close_session(storage);

  for (size_t i = 0; i < active_port_count; ++i) {
    ESP_LOGI(TAG, "Captured %u %s UART bytes into the RAM pipeline",
             (unsigned)s_reference_ports[i].total_bytes,
             board->ports[s_reference_ports[i].board_index].name);
  }
  emit_file_artifact_hex("session.bin", storage_service_binary_path(storage));
  emit_file_artifact_hex("status.log", storage_service_status_path(storage));
  emit_file_artifact_hex("config.txt", storage_service_config_path(storage));
  runtime_banner_log_ready(select_capture_case_name(
      config, s_sync_capture_service.drained_events,
      fault_manager_event_count(&controller->fault_manager)));
  return ESP_OK;
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

  runtime_banner_log_startup();
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

  if (!session_controller_request_start(&session_controller, &runtime_config)) {
    ESP_LOGE(TAG, "Automatic session start rejected");
    abort();
  }
  ESP_ERROR_CHECK(session_controller_mark_recording(&session_controller));
  ESP_ERROR_CHECK(run_reference_sd_logger_case(
      board, &runtime_config, &session_controller, &s_storage_service));
  storage_service_unmount(&s_storage_service);
  runtime_banner_start_health_task();
  runtime_banner_log_ready("platform_smoke");
}
