#include "uart_capture_service.h"

#include <string.h>

size_t uart_capture_required_retention_bytes(uint32_t baud_rate,
                                             uint32_t retention_ms) {
  const uint64_t bits_per_byte = 10U;
  const uint64_t ms_per_second = 1000U;
  const uint64_t numerator = ((uint64_t)baud_rate * (uint64_t)retention_ms) +
                             ((bits_per_byte * ms_per_second) - 1U);

  if (baud_rate == 0U || retention_ms == 0U) {
    return 0U;
  }

  return (size_t)(numerator / (bits_per_byte * ms_per_second));
}

static size_t uart_capture_max_chunk_bytes(void) {
  return RECORD_BUFFER_CAPACITY_BYTES - sizeof(binary_record_header_t) -
         sizeof(uart_data_record_payload_prefix_t);
}

static void uart_capture_set_fault(uart_capture_service_t* service) {
  if (service == NULL) {
    return;
  }

  service->pending_fault_valid = true;
  service->pending_fault.code = FAULT_CODE_STORAGE_BACKPRESSURE;
  service->pending_fault.severity = FAULT_SEVERITY_RECOVERABLE;
}

static void uart_capture_reset_chunk(uart_capture_service_t* service) {
  service->read_index = service->write_index;
  service->pending_bytes = 0U;
  service->stats.pending_bytes = 0U;
  service->active_chunk_start_us = 0U;
  service->chunk_open = false;
}

static void uart_capture_copy_out(const uart_capture_service_t* service,
                                  uint8_t* out, size_t length) {
  size_t first_span;

  if (length == 0U) {
    return;
  }

  first_span = service->capacity_bytes - service->read_index;
  if (first_span > length) {
    first_span = length;
  }

  memcpy(out, service->storage + service->read_index, first_span);
  if (first_span < length) {
    memcpy(out + first_span, service->storage, length - first_span);
  }
}

esp_err_t uart_capture_service_init(uart_capture_service_t* service,
                                    port_id_t port_id, uint8_t* storage,
                                    size_t capacity_bytes,
                                    binary_log_pipeline_t* pipeline) {
  if (service == NULL || storage == NULL || pipeline == NULL ||
      capacity_bytes == 0U || port_id == PORT_ID_NONE) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(service, 0, sizeof(*service));
  service->storage = storage;
  service->capacity_bytes = capacity_bytes;
  service->port_id = port_id;
  service->pipeline = pipeline;
  service->stats.port_id = port_id;
  return ESP_OK;
}

esp_err_t uart_capture_service_publish_pending(uart_capture_service_t* service,
                                               record_buffer_t* out_record) {
  record_buffer_t record = {};
  fault_event_t overflow_fault = {};
  uint8_t chunk_bytes[RECORD_BUFFER_CAPACITY_BYTES] = {0};
  esp_err_t err;

  if (service == NULL || service->pipeline == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (service->pending_bytes == 0U) {
    return ESP_OK;
  }

  uart_capture_copy_out(service, chunk_bytes, service->pending_bytes);
  err = record_builder_build_uart_data(
      (uint32_t)service->port_id, service->active_chunk_start_us, chunk_bytes,
      service->pending_bytes, &record);
  if (err != ESP_OK) {
    return err;
  }

  err = binary_log_pipeline_append(service->pipeline, &record, &overflow_fault);
  if (err != ESP_OK) {
    uart_capture_set_fault(service);
    return err;
  }

  if (out_record != NULL) {
    *out_record = record;
  }

  service->stats.published_chunks += 1U;
  uart_capture_reset_chunk(service);
  return ESP_OK;
}

esp_err_t uart_capture_service_on_rx_bytes(uart_capture_service_t* service,
                                           port_id_t port_id,
                                           uint64_t timestamp_us,
                                           const uint8_t* bytes,
                                           size_t length) {
  const size_t max_chunk_bytes = uart_capture_max_chunk_bytes();
  size_t offset = 0U;

  if (service == NULL || bytes == NULL || length == 0U ||
      service->storage == NULL || service->pipeline == NULL ||
      port_id != service->port_id) {
    return ESP_ERR_INVALID_ARG;
  }

  if (service->capacity_bytes < max_chunk_bytes &&
      length > service->capacity_bytes) {
    uart_capture_set_fault(service);
    return ESP_ERR_NO_MEM;
  }

  while (offset < length) {
    size_t chunk_capacity;
    size_t writable;
    size_t first_span;

    if (!service->chunk_open) {
      service->active_chunk_start_us = timestamp_us;
      service->chunk_open = true;
    }

    if (service->pending_bytes == max_chunk_bytes) {
      esp_err_t publish_err =
          uart_capture_service_publish_pending(service, NULL);
      if (publish_err != ESP_OK) {
        uart_capture_set_fault(service);
        return publish_err;
      }
      service->active_chunk_start_us = timestamp_us;
      service->chunk_open = true;
    }

    chunk_capacity = service->capacity_bytes - service->pending_bytes;
    writable = max_chunk_bytes - service->pending_bytes;
    if (writable > chunk_capacity) {
      writable = chunk_capacity;
    }
    if (writable == 0U) {
      uart_capture_set_fault(service);
      return ESP_ERR_NO_MEM;
    }
    if (writable > length - offset) {
      writable = length - offset;
    }

    first_span = service->capacity_bytes - service->write_index;
    if (first_span > writable) {
      first_span = writable;
    }

    memcpy(service->storage + service->write_index, bytes + offset, first_span);
    if (first_span < writable) {
      memcpy(service->storage, bytes + offset + first_span,
             writable - first_span);
    }

    service->write_index =
        (service->write_index + writable) % service->capacity_bytes;
    service->pending_bytes += writable;
    service->stats.total_bytes_captured += writable;
    service->stats.pending_bytes = service->pending_bytes;
    offset += writable;
  }

  return ESP_OK;
}

bool uart_capture_service_take_pending_fault(uart_capture_service_t* service,
                                             fault_event_t* out_fault) {
  if (service == NULL || out_fault == NULL || !service->pending_fault_valid) {
    return false;
  }

  *out_fault = service->pending_fault;
  service->pending_fault_valid = false;
  return true;
}

esp_err_t uart_capture_service_get_stats(const uart_capture_service_t* service,
                                         port_id_t port_id,
                                         uart_capture_stats_t* out_stats) {
  if (service == NULL || out_stats == NULL || port_id == PORT_ID_NONE ||
      service->port_id != port_id) {
    return ESP_ERR_INVALID_ARG;
  }

  *out_stats = service->stats;
  out_stats->pending_bytes = service->pending_bytes;
  return ESP_OK;
}
