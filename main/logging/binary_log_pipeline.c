#include "binary_log_pipeline.h"

#include <string.h>

void binary_log_pipeline_init(binary_log_pipeline_t* pipeline, uint8_t* storage,
                              size_t capacity_bytes) {
  if (pipeline == NULL) {
    return;
  }

  pipeline->storage = storage;
  pipeline->capacity_bytes = capacity_bytes;
  pipeline->pending_bytes = 0U;
}

esp_err_t binary_log_pipeline_append(binary_log_pipeline_t* pipeline,
                                     const record_buffer_t* record,
                                     fault_event_t* overflow_fault) {
  if (pipeline == NULL || record == NULL || pipeline->storage == NULL ||
      record->length == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  if (record->length > pipeline->capacity_bytes ||
      pipeline->pending_bytes > pipeline->capacity_bytes - record->length) {
    if (overflow_fault != NULL) {
      overflow_fault->code = FAULT_CODE_STORAGE_BACKPRESSURE;
      overflow_fault->severity = FAULT_SEVERITY_RECOVERABLE;
    }
    return ESP_ERR_NO_MEM;
  }

  memcpy(pipeline->storage + pipeline->pending_bytes, record->bytes,
         record->length);
  pipeline->pending_bytes += record->length;
  return ESP_OK;
}

esp_err_t binary_log_pipeline_flush(binary_log_pipeline_t* pipeline,
                                    uint8_t* out, size_t out_capacity,
                                    size_t* bytes_written) {
  size_t chunk_size = 0U;

  if (bytes_written != NULL) {
    *bytes_written = 0U;
  }

  if (pipeline == NULL || out == NULL || bytes_written == NULL ||
      out_capacity == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  if (pipeline->storage == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (pipeline->pending_bytes == 0U) {
    return ESP_OK;
  }

  chunk_size = pipeline->pending_bytes < out_capacity ? pipeline->pending_bytes
                                                      : out_capacity;
  memcpy(out, pipeline->storage, chunk_size);
  pipeline->pending_bytes -= chunk_size;
  if (pipeline->pending_bytes > 0U) {
    memmove(pipeline->storage, pipeline->storage + chunk_size,
            pipeline->pending_bytes);
  }

  *bytes_written = chunk_size;
  return ESP_OK;
}

size_t binary_log_pipeline_pending_bytes(
    const binary_log_pipeline_t* pipeline) {
  if (pipeline == NULL) {
    return 0U;
  }

  return pipeline->pending_bytes;
}
