#include "binary_log_pipeline.h"

#include "log_pipeline_base.h"

static log_pipeline_base_t binary_log_pipeline_to_base(
    const binary_log_pipeline_t* pipeline) {
  log_pipeline_base_t base = {};
  if (pipeline == NULL) {
    return base;
  }

  base.storage = pipeline->storage;
  base.capacity_bytes = pipeline->capacity_bytes;
  base.pending_bytes = pipeline->pending_bytes;
  return base;
}

static void binary_log_pipeline_apply_base(binary_log_pipeline_t* pipeline,
                                           const log_pipeline_base_t* base) {
  if (pipeline == NULL || base == NULL) {
    return;
  }

  pipeline->storage = base->storage;
  pipeline->capacity_bytes = base->capacity_bytes;
  pipeline->pending_bytes = base->pending_bytes;
}

void binary_log_pipeline_init(binary_log_pipeline_t* pipeline, uint8_t* storage,
                              size_t capacity_bytes) {
  log_pipeline_base_t base = {};
  log_pipeline_base_init(&base, storage, capacity_bytes);
  binary_log_pipeline_apply_base(pipeline, &base);
}

esp_err_t binary_log_pipeline_append(binary_log_pipeline_t* pipeline,
                                     const record_buffer_t* record,
                                     fault_event_t* overflow_fault) {
  log_pipeline_base_t base = {};
  esp_err_t status = ESP_OK;

  if (pipeline == NULL || record == NULL || pipeline->storage == NULL ||
      record->length == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  base = binary_log_pipeline_to_base(pipeline);
  status = log_pipeline_base_append(&base, record->bytes, record->length);
  if (status == ESP_ERR_NO_MEM) {
    if (overflow_fault != NULL) {
      overflow_fault->code = FAULT_CODE_STORAGE_BACKPRESSURE;
      overflow_fault->severity = FAULT_SEVERITY_RECOVERABLE;
    }
    return status;
  }
  if (status != ESP_OK) {
    return status;
  }

  binary_log_pipeline_apply_base(pipeline, &base);
  return ESP_OK;
}

esp_err_t binary_log_pipeline_flush(binary_log_pipeline_t* pipeline,
                                    uint8_t* out, size_t out_capacity,
                                    size_t* bytes_written) {
  log_pipeline_base_t base = binary_log_pipeline_to_base(pipeline);
  const esp_err_t status =
      log_pipeline_base_flush(&base, out, out_capacity, bytes_written);
  binary_log_pipeline_apply_base(pipeline, &base);
  return status;
}

size_t binary_log_pipeline_pending_bytes(
    const binary_log_pipeline_t* pipeline) {
  const log_pipeline_base_t base = binary_log_pipeline_to_base(pipeline);
  return log_pipeline_base_pending_bytes(&base);
}
