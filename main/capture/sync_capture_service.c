#include "sync_capture_service.h"

#include <string.h>

esp_err_t sync_capture_service_init(sync_capture_service_t* service,
                                    sync_edge_event_t* storage,
                                    size_t capacity) {
  if (service == NULL || storage == NULL || capacity == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  memset(service, 0, sizeof(*service));
  service->storage = storage;
  service->capacity = capacity;
  return ESP_OK;
}

esp_err_t sync_capture_service_publish_isr(sync_capture_service_t* service,
                                           port_id_t port_id,
                                           uint64_t timestamp_us,
                                           bool level_high) {
  sync_edge_event_t* slot = NULL;

  if (service == NULL || service->storage == NULL || service->capacity == 0U ||
      port_id == PORT_ID_NONE) {
    return ESP_ERR_INVALID_ARG;
  }

  if (service->count == service->capacity) {
    service->overflowed = true;
    return ESP_ERR_NO_MEM;
  }

  slot = &service->storage[service->tail];
  slot->port_id = port_id;
  slot->timestamp_us = timestamp_us;
  slot->level_high = level_high;
  service->last_timestamp_us = timestamp_us;
  service->tail = (service->tail + 1U) % service->capacity;
  service->count += 1U;
  return ESP_OK;
}

esp_err_t sync_capture_service_drain(sync_capture_service_t* service,
                                     binary_log_pipeline_t* pipeline,
                                     fault_manager_t* faults) {
  record_buffer_t record = {};

  if (service == NULL || pipeline == NULL || faults == NULL ||
      service->storage == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  while (service->count > 0U) {
    fault_event_t overflow_fault = {};
    const sync_edge_event_t event = service->storage[service->head];
    esp_err_t err = record_builder_build_sync_edge(
        (uint32_t)event.port_id, event.timestamp_us, event.level_high, &record);
    if (err != ESP_OK) {
      return err;
    }

    err = binary_log_pipeline_append(pipeline, &record, &overflow_fault);
    if (err != ESP_OK) {
      fault_manager_publish(faults, &overflow_fault);
      return err;
    }

    service->head = (service->head + 1U) % service->capacity;
    service->count -= 1U;
    service->drained_events += 1U;
  }

  if (service->overflowed) {
    record_buffer_t fault_record = {};
    const fault_event_t fault = {
        .code = FAULT_CODE_CAPTURE_OVERFLOW,
        .severity = FAULT_SEVERITY_RECOVERABLE,
    };
    fault_manager_publish(faults, &fault);
    if (record_builder_build_fault_event(service->last_timestamp_us, 0U, &fault,
                                         fault_manager_health(faults),
                                         &fault_record) == ESP_OK) {
      fault_event_t append_fault = {};
      const esp_err_t append_err =
          binary_log_pipeline_append(pipeline, &fault_record, &append_fault);
      if (append_err != ESP_OK) {
        fault_manager_publish(faults, &append_fault);
        return append_err;
      }
    }
    service->overflowed = false;
  }

  return ESP_OK;
}
