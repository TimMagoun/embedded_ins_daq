#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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
#ifndef ESP_FAIL
#define ESP_FAIL -1
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define STORAGE_SERVICE_PATH_CAPACITY 256U

typedef struct {
  char base_path[STORAGE_SERVICE_PATH_CAPACITY];
  char session_dir_path[STORAGE_SERVICE_PATH_CAPACITY];
  char binary_path[STORAGE_SERVICE_PATH_CAPACITY];
  char status_path[STORAGE_SERVICE_PATH_CAPACITY];
  FILE* binary_file;
  FILE* status_file;
  bool mounted;
  bool session_open;
  bool host_mode;
  int sdmmc_slot;
} storage_service_t;

void storage_service_init(storage_service_t* service, int sdmmc_slot);
esp_err_t storage_service_init_for_host(storage_service_t* service,
                                        const char* root_path);
esp_err_t storage_service_mount(storage_service_t* service);
esp_err_t storage_service_open_session(storage_service_t* service,
                                       const session_info_t* session);
esp_err_t storage_service_write_binary_block(storage_service_t* service,
                                             const uint8_t* data,
                                             size_t length);
esp_err_t storage_service_write_status_message(storage_service_t* service,
                                               const char* message);
void storage_service_close_session(storage_service_t* service);
void storage_service_unmount(storage_service_t* service);
const char* storage_service_session_dir(const storage_service_t* service);
const char* storage_service_binary_path(const storage_service_t* service);
const char* storage_service_status_path(const storage_service_t* service);

#ifdef __cplusplus
}
#endif
