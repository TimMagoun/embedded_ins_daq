#include "log_pipeline_base.h"

#include <string.h>

void log_pipeline_base_init(log_pipeline_base_t* pipeline, void* storage,
                            size_t capacity_bytes) {
  if (pipeline == NULL) {
    return;
  }

  pipeline->storage = storage;
  pipeline->capacity_bytes = capacity_bytes;
  pipeline->pending_bytes = 0U;
}

esp_err_t log_pipeline_base_append(log_pipeline_base_t* pipeline,
                                   const void* bytes, size_t length) {
  if (pipeline == NULL || pipeline->storage == NULL || bytes == NULL ||
      length == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  if (length > pipeline->capacity_bytes ||
      pipeline->pending_bytes > pipeline->capacity_bytes - length) {
    return ESP_ERR_NO_MEM;
  }

  memcpy(pipeline->storage + pipeline->pending_bytes, bytes, length);
  pipeline->pending_bytes += length;
  return ESP_OK;
}

esp_err_t log_pipeline_base_flush(log_pipeline_base_t* pipeline, void* out,
                                  size_t out_capacity, size_t* bytes_written) {
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

size_t log_pipeline_base_pending_bytes(const log_pipeline_base_t* pipeline) {
  if (pipeline == NULL) {
    return 0U;
  }

  return pipeline->pending_bytes;
}
