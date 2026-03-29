#include "record_builder.h"

#include <string.h>

static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t length) {
  uint32_t value = crc;

  for (size_t i = 0; i < length; ++i) {
    value ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = (uint32_t)(-(int)(value & 1u));
      value = (value >> 1) ^ (0xedb88320u & mask);
    }
  }

  return value;
}

static uint32_t crc32_bytes(const uint8_t* data, size_t length) {
  return ~crc32_update(0xffffffffu, data, length);
}

static esp_err_t build_record(uint16_t record_type, uint64_t timestamp_us,
                              uint32_t source_id, const void* payload,
                              size_t payload_length, record_buffer_t* out) {
  binary_record_header_t header = {};
  uint8_t* payload_bytes = NULL;
  uint32_t crc = 0u;

  if (payload_length > 0U && payload == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (out == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (sizeof(header) + payload_length > sizeof(out->bytes)) {
    return ESP_ERR_NO_MEM;
  }

  header.record_type = record_type;
  header.record_version = RECORD_FORMAT_VERSION;
  header.payload_length = (uint32_t)payload_length;
  header.timestamp_us = timestamp_us;
  header.source_id = source_id;

  memcpy(out->bytes, &header, sizeof(header));
  if (payload_length > 0U) {
    memcpy(out->bytes + sizeof(header), payload, payload_length);
  }

  payload_bytes = out->bytes + sizeof(header);
  crc = crc32_bytes(out->bytes, offsetof(binary_record_header_t, crc32));
  if (payload_length > 0U) {
    crc = ~crc32_update(~crc, payload_bytes, payload_length);
  }

  header.crc32 = crc;
  memcpy(out->bytes, &header, sizeof(header));
  out->length = sizeof(header) + payload_length;
  return ESP_OK;
}

uint32_t record_builder_config_hash(const runtime_config_t* config) {
  uint32_t hash = 2166136261u;

  if (config == NULL) {
    return 0u;
  }

  hash = (hash ^ (uint32_t)config->port_count) * 16777619u;
  for (size_t i = 0; i < config->port_count; ++i) {
    const runtime_port_config_t* port = &config->ports[i];
    hash = (hash ^ (uint32_t)(port->enabled ? 1u : 0u)) * 16777619u;
    hash = (hash ^ (uint32_t)port->uart_port) * 16777619u;
    hash = (hash ^ (uint32_t)port->baud_rate) * 16777619u;
    hash = (hash ^ (uint32_t)port->timing_mode) * 16777619u;
    hash = (hash ^ (uint32_t)port->sync_edge_mode) * 16777619u;
    hash = (hash ^ (uint32_t)(port->enable_sync_input ? 1u : 0u)) * 16777619u;
    hash = (hash ^ port->trigger_period_us) * 16777619u;
    hash = (hash ^ port->trigger_pulse_width_us) * 16777619u;
  }

  return hash;
}

esp_err_t record_builder_build_session_start(const session_info_t* session,
                                             const runtime_config_t* config,
                                             record_buffer_t* out) {
  session_start_record_payload_t payload = {};

  if (session == NULL || config == NULL || out == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  payload.session_id = session->session_id;
  payload.config_hash = record_builder_config_hash(config);

  for (size_t i = 0; i < config->port_count && i < 32U; ++i) {
    if (!config->ports[i].enabled) {
      continue;
    }

    payload.enabled_port_mask |= (uint32_t)(1u << i);
    payload.enabled_port_count += 1u;
  }

  return build_record(RECORD_TYPE_SESSION_START, session->start_timestamp_us,
                      0u, &payload, sizeof(payload), out);
}

esp_err_t record_builder_build_fault_event(uint64_t timestamp_us,
                                           uint32_t source_id,
                                           const fault_event_t* event,
                                           health_status_t health,
                                           record_buffer_t* out) {
  fault_event_record_payload_t payload = {};

  if (event == NULL || out == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  payload.fault_code = (uint32_t)event->code;
  payload.fault_severity = (uint32_t)event->severity;
  payload.health_status = (uint32_t)health;

  return build_record(RECORD_TYPE_FAULT_EVENT, timestamp_us, source_id,
                      &payload, sizeof(payload), out);
}

esp_err_t record_builder_build_uart_data(uint32_t source_id,
                                         uint64_t first_byte_timestamp_us,
                                         const uint8_t* bytes, size_t length,
                                         record_buffer_t* out) {
  uart_data_record_payload_prefix_t prefix = {};
  uint8_t payload[RECORD_BUFFER_CAPACITY_BYTES] = {0};

  if (bytes == NULL || out == NULL || length == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  if (sizeof(prefix) + length > sizeof(payload)) {
    return ESP_ERR_NO_MEM;
  }

  prefix.data_length = (uint32_t)length;
  memcpy(payload, &prefix, sizeof(prefix));
  memcpy(payload + sizeof(prefix), bytes, length);

  return build_record(RECORD_TYPE_UART_DATA, first_byte_timestamp_us, source_id,
                      payload, sizeof(prefix) + length, out);
}

esp_err_t record_builder_build_sync_edge(uint32_t source_id,
                                         uint64_t timestamp_us, bool level_high,
                                         record_buffer_t* out) {
  sync_edge_record_payload_t payload = {};

  if (out == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  payload.edge_polarity = level_high ? 1u : 0u;
  return build_record(RECORD_TYPE_SYNC_EDGE, timestamp_us, source_id, &payload,
                      sizeof(payload), out);
}
