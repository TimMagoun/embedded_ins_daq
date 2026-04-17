#pragma once

#include <stddef.h>
#include <stdint.h>

#include "record_builder.h"

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

typedef struct {
  uint8_t* storage;
  size_t capacity_bytes;
  size_t pending_bytes;
} binary_log_pipeline_t;

void binary_log_pipeline_init(binary_log_pipeline_t* pipeline, uint8_t* storage,
                              size_t capacity_bytes);

esp_err_t binary_log_pipeline_append(binary_log_pipeline_t* pipeline,
                                     const record_buffer_t* record,
                                     fault_event_t* overflow_fault);

esp_err_t binary_log_pipeline_flush(binary_log_pipeline_t* pipeline,
                                    uint8_t* out, size_t out_capacity,
                                    size_t* bytes_written);

size_t binary_log_pipeline_pending_bytes(const binary_log_pipeline_t* pipeline);

#ifdef __cplusplus
}
#endif
