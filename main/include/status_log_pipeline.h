#pragma once

#include <stddef.h>

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
  char* storage;
  size_t capacity_bytes;
  size_t length;
} status_log_pipeline_t;

void status_log_pipeline_init(status_log_pipeline_t* pipeline, char* storage,
                              size_t capacity_bytes);

esp_err_t status_log_pipeline_append(status_log_pipeline_t* pipeline,
                                     const char* message);

esp_err_t status_log_pipeline_flush(status_log_pipeline_t* pipeline, char* out,
                                    size_t out_capacity, size_t* bytes_written);

size_t status_log_pipeline_pending_bytes(const status_log_pipeline_t* pipeline);

#ifdef __cplusplus
}
#endif
