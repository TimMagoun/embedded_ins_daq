#include "record_builder.h"

#include <limits.h>
#include <string.h>

static const uint32_t kCrc32NibbleTable[16] = {
    0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
    0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
    0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
    0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu,
};
static const uint32_t kFnvOffsetBasis = 2166136261u;
static const uint32_t kFnvPrime = 16777619u;
static const uint32_t kRecordPortMaskBits =
    sizeof(((session_start_record_payload_t*)0)->enabled_port_mask) * CHAR_BIT;

_Static_assert(sizeof(binary_record_header_t) < RECORD_BUFFER_CAPACITY_BYTES,
               "record header must fit in the output buffer");
_Static_assert(sizeof(binary_record_header_t) +
                       sizeof(session_start_record_payload_t) <=
                   RECORD_BUFFER_CAPACITY_BYTES,
               "session start records must fit in the output buffer");
_Static_assert(sizeof(binary_record_header_t) +
                       sizeof(fault_event_record_payload_t) <=
                   RECORD_BUFFER_CAPACITY_BYTES,
               "fault event records must fit in the output buffer");
_Static_assert(sizeof(binary_record_header_t) +
                       sizeof(sync_edge_record_payload_t) <=
                   RECORD_BUFFER_CAPACITY_BYTES,
               "sync edge records must fit in the output buffer");

static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t length) {
  uint32_t value = crc;

  for (size_t i = 0; i < length; ++i) {
    value ^= data[i];
    value = (value >> 4) ^ kCrc32NibbleTable[value & 0x0fu];
    value = (value >> 4) ^ kCrc32NibbleTable[value & 0x0fu];
  }

  return value;
}

static uint32_t crc32_bytes(const uint8_t* data, size_t length) {
  return ~crc32_update(0xffffffffu, data, length);
}

static uint32_t fnv1a_mix(uint32_t hash, uint32_t value) {
  return (hash ^ value) * kFnvPrime;
}

static esp_err_t build_record(uint8_t record_type, uint64_t timestamp_us,
                              uint32_t source_id, const void* payload,
                              size_t payload_length, record_buffer_t* out) {
  binary_record_header_t header = {};
  uint8_t* payload_bytes = NULL;
  uint32_t crc = 0u;

  if (source_id > UINT8_MAX || payload_length > UINT16_MAX) {
    return ESP_ERR_INVALID_ARG;
  }

  header.record_type = record_type;
  header.record_version = RECORD_FORMAT_VERSION;
  header.payload_length = (uint16_t)payload_length;
  header.timestamp_us = timestamp_us;
  header.source_id = (uint8_t)source_id;

  /* Source and destination never overlap in these record writes. */
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
  uint32_t hash = kFnvOffsetBasis;

  if (config == NULL) {
    return 0u;
  }

  hash = fnv1a_mix(hash, (uint32_t)BOARD_PORT_COUNT);
  for (size_t i = 0; i < BOARD_PORT_COUNT; ++i) {
    const runtime_port_config_t* port = &config->ports[i];
    hash = fnv1a_mix(hash, (uint32_t)(port->enabled ? 1u : 0u));
    hash = fnv1a_mix(hash, (uint32_t)port->baud_rate);
    hash = fnv1a_mix(hash, (uint32_t)port->timing_mode);
    hash = fnv1a_mix(hash, (uint32_t)port->sync_edge_mode);
    hash = fnv1a_mix(hash, port->trigger_period_us);
    hash = fnv1a_mix(hash, port->trigger_pulse_width_us);
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

  for (size_t i = 0; i < BOARD_PORT_COUNT && i < kRecordPortMaskBits; ++i) {
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

  if (bytes == NULL || out == NULL || length == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  if (sizeof(binary_record_header_t) + sizeof(prefix) + length >
      sizeof(out->bytes)) {
    return ESP_ERR_NO_MEM;
  }

  prefix.data_length = (uint16_t)length;
  /* The caller-provided buffer is distinct from the output record buffer. */
  memcpy(out->bytes + sizeof(binary_record_header_t), &prefix, sizeof(prefix));
  memcpy(out->bytes + sizeof(binary_record_header_t) + sizeof(prefix), bytes,
         length);

  return build_record(RECORD_TYPE_UART_DATA, first_byte_timestamp_us, source_id,
                      out->bytes + sizeof(binary_record_header_t),
                      sizeof(prefix) + length, out);
}

esp_err_t record_builder_build_sync_edge(uint32_t source_id,
                                         uint64_t timestamp_us, bool level_high,
                                         record_buffer_t* out) {
  sync_edge_record_payload_t payload = {};

  if (out == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  payload.edge_polarity = level_high;
  return build_record(RECORD_TYPE_SYNC_EDGE, timestamp_us, source_id, &payload,
                      sizeof(payload), out);
}
