#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fault_manager.h"
#include "runtime_config.h"

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

#define RECORD_FORMAT_VERSION 1u
#define RECORD_BUFFER_CAPACITY_BYTES 512u

typedef enum {
  RECORD_TYPE_SESSION_START = 1,
  RECORD_TYPE_FAULT_EVENT = 2,
  RECORD_TYPE_UART_DATA = 3,
  RECORD_TYPE_SYNC_EDGE = 4,
} record_type_t;

typedef struct {
  uint16_t record_type;
  uint16_t record_version;
  uint32_t payload_length;
  uint64_t timestamp_us;
  uint32_t source_id;
  uint32_t crc32;
} binary_record_header_t;

typedef struct {
  uint8_t bytes[RECORD_BUFFER_CAPACITY_BYTES];
  size_t length;
} record_buffer_t;

typedef struct {
  uint32_t session_id;
  uint64_t start_timestamp_us;
} session_info_t;

typedef struct {
  uint32_t session_id;
  uint32_t config_hash;
  uint32_t enabled_port_mask;
  uint32_t enabled_port_count;
} session_start_record_payload_t;

typedef struct {
  uint32_t fault_code;
  uint32_t fault_severity;
  uint32_t health_status;
  uint32_t reserved;
} fault_event_record_payload_t;

typedef struct {
  uint32_t data_length;
} uart_data_record_payload_prefix_t;

typedef struct {
  uint32_t edge_polarity;
  uint32_t reserved;
} sync_edge_record_payload_t;

uint32_t record_builder_config_hash(const runtime_config_t* config);

esp_err_t record_builder_build_session_start(const session_info_t* session,
                                             const runtime_config_t* config,
                                             record_buffer_t* out);

esp_err_t record_builder_build_fault_event(uint64_t timestamp_us,
                                           uint32_t source_id,
                                           const fault_event_t* event,
                                           health_status_t health,
                                           record_buffer_t* out);

esp_err_t record_builder_build_uart_data(uint32_t source_id,
                                         uint64_t first_byte_timestamp_us,
                                         const uint8_t* bytes, size_t length,
                                         record_buffer_t* out);

esp_err_t record_builder_build_sync_edge(uint32_t source_id,
                                         uint64_t timestamp_us, bool level_high,
                                         record_buffer_t* out);

#ifdef __cplusplus
}
#endif
