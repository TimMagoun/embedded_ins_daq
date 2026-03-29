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

/* Carries one observed sync edge prior to record encoding. */
typedef struct {
  port_id_t port_id;
  uint64_t timestamp_us;
  bool level_high;
} sync_edge_event_t;

/* Queues sync edges from ISR context until the logging task drains them. */
typedef struct {
  sync_edge_event_t* storage;
  size_t capacity;
  size_t head;
  size_t tail;
  size_t count;
  size_t drained_events;
  uint64_t last_timestamp_us;
  bool overflowed;
} sync_capture_service_t;

/* Initializes the sync-edge queue service with caller-owned storage. */
esp_err_t sync_capture_service_init(sync_capture_service_t* service,
                                    sync_edge_event_t* storage,
                                    size_t capacity);

/* Publishes one observed sync edge from ISR context into the queue. */
esp_err_t sync_capture_service_publish_isr(sync_capture_service_t* service,
                                           port_id_t port_id,
                                           uint64_t timestamp_us,
                                           bool level_high);

/* Drains queued sync edges into the binary log pipeline and reports faults. */
esp_err_t sync_capture_service_drain(sync_capture_service_t* service,
                                     binary_log_pipeline_t* pipeline,
                                     fault_manager_t* faults);

#ifdef __cplusplus
}
#endif
