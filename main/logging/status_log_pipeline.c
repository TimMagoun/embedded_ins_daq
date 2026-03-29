#include "status_log_pipeline.h"

#include <string.h>

void status_log_pipeline_init(status_log_pipeline_t* pipeline, char* storage,
                              size_t capacity_bytes) {
  if (pipeline == NULL) {
    return;
  }

  pipeline->storage = storage;
  pipeline->capacity_bytes = capacity_bytes;
  pipeline->length = 0U;
  if (storage != NULL && capacity_bytes > 0U) {
    storage[0] = '\0';
  }
}

esp_err_t status_log_pipeline_append(status_log_pipeline_t* pipeline,
                                     const char* message) {
  const size_t message_length = message == NULL ? 0U : strlen(message);

  if (pipeline == NULL || pipeline->storage == NULL || message == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (message_length + 1U > pipeline->capacity_bytes - pipeline->length) {
    return ESP_ERR_NO_MEM;
  }

  memcpy(pipeline->storage + pipeline->length, message, message_length);
  pipeline->length += message_length;
  pipeline->storage[pipeline->length++] = '\n';
  if (pipeline->length < pipeline->capacity_bytes) {
    pipeline->storage[pipeline->length] = '\0';
  }
  return ESP_OK;
}

esp_err_t status_log_pipeline_flush(status_log_pipeline_t* pipeline, char* out,
                                    size_t out_capacity,
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

  chunk_size =
      pipeline->length < out_capacity ? pipeline->length : out_capacity;
  memcpy(out, pipeline->storage, chunk_size);
  pipeline->length -= chunk_size;
  if (pipeline->length > 0U) {
    memmove(pipeline->storage, pipeline->storage + chunk_size,
            pipeline->length);
  }
  if (pipeline->capacity_bytes > 0U) {
    pipeline->storage[pipeline->length] = '\0';
  }

  *bytes_written = chunk_size;
  return ESP_OK;
}

size_t status_log_pipeline_pending_bytes(
    const status_log_pipeline_t* pipeline) {
  if (pipeline == NULL) {
    return 0U;
  }

  return pipeline->length;
}
