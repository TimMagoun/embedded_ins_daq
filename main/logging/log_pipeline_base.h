#pragma once

#include <stddef.h>
#include <stdint.h>

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
} log_pipeline_base_t;

void log_pipeline_base_init(log_pipeline_base_t* pipeline, void* storage,
                            size_t capacity_bytes);

esp_err_t log_pipeline_base_append(log_pipeline_base_t* pipeline,
                                   const void* bytes, size_t length);

esp_err_t log_pipeline_base_flush(log_pipeline_base_t* pipeline, void* out,
                                  size_t out_capacity, size_t* bytes_written);

size_t log_pipeline_base_pending_bytes(const log_pipeline_base_t* pipeline);

#ifdef __cplusplus
}
#endif
