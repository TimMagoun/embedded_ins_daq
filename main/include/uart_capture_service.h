#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "binary_log_pipeline.h"
#include "fault_manager.h"
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
#ifndef ESP_ERR_NO_MEM
#define ESP_ERR_NO_MEM 0x105
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Summarizes the isolated capture accounting for one logical UART port. */
typedef struct {
  port_id_t port_id;
  size_t total_bytes_captured;
  size_t pending_bytes;
  size_t published_chunks;
} uart_capture_stats_t;

/* Tracks one in-flight UART chunk before it is encoded into the RAM pipeline.
 */
typedef struct {
  uint8_t* storage;
  size_t capacity_bytes;
  size_t write_index;
  size_t read_index;
  size_t pending_bytes;
  uint64_t active_chunk_start_us;
  bool chunk_open;
  port_id_t port_id;
  binary_log_pipeline_t* pipeline;
  bool pending_fault_valid;
  fault_event_t pending_fault;
  uart_capture_stats_t stats;
} uart_capture_service_t;

/* Initializes one-port UART chunk capture backed by the provided ring storage.
 */
esp_err_t uart_capture_service_init(uart_capture_service_t* service,
                                    port_id_t port_id, uint8_t* storage,
                                    size_t capacity_bytes,
                                    binary_log_pipeline_t* pipeline);

/* Appends newly observed UART RX bytes into the active chunk. */
esp_err_t uart_capture_service_on_rx_bytes(uart_capture_service_t* service,
                                           port_id_t port_id,
                                           uint64_t timestamp_us,
                                           const uint8_t* bytes, size_t length);

/* Publishes the active chunk into the RAM log pipeline. */
esp_err_t uart_capture_service_publish_pending(uart_capture_service_t* service,
                                               record_buffer_t* out_record);

/* Returns and clears the most recent recoverable capture fault, if present. */
bool uart_capture_service_take_pending_fault(uart_capture_service_t* service,
                                             fault_event_t* out_fault);

/* Reports isolated accounting for the selected capture port. */
esp_err_t uart_capture_service_get_stats(const uart_capture_service_t* service,
                                         port_id_t port_id,
                                         uart_capture_stats_t* out_stats);

/* Estimates UART retention bytes for the requested baud rate and window. */
size_t uart_capture_required_retention_bytes(uint32_t baud_rate,
                                             uint32_t retention_ms);

#ifdef __cplusplus
}
#endif
