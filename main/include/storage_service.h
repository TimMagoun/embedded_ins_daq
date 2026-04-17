#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "fault_manager.h"
#include "record_builder.h"
#include "runtime_config.h"

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

/* Owns session folder and artifact files on host or SD-backed storage. */
typedef struct {
  char base_path[STORAGE_SERVICE_PATH_CAPACITY];
  char session_dir_path[STORAGE_SERVICE_PATH_CAPACITY];
  char binary_path[STORAGE_SERVICE_PATH_CAPACITY];
  char status_path[STORAGE_SERVICE_PATH_CAPACITY];
  char config_path[STORAGE_SERVICE_PATH_CAPACITY];
  FILE* binary_file;
  FILE* status_file;
  bool mounted;
  bool session_open;
  bool host_mode;
  bool fail_next_write_for_host;
  int sdmmc_slot;
  void* sd_card;
  void* sd_pwr_ctrl_handle;
  bool pending_fault_valid;
  fault_event_t pending_fault;
} storage_service_t;

/* Initializes the service for device-side SD-backed storage. */
void storage_service_init(storage_service_t* service, int sdmmc_slot);

/* Initializes the service for host tests rooted at the provided path. */
esp_err_t storage_service_init_for_host(storage_service_t* service,
                                        const char* root_path);

/* Mounts the underlying storage root. */
esp_err_t storage_service_mount(storage_service_t* service);

/* Opens a new session folder plus `session.bin` and `status.log` files. */
esp_err_t storage_service_open_session(storage_service_t* service,
                                       const session_info_t* session);

/* Appends one binary block to the active `session.bin`. */
esp_err_t storage_service_write_binary_block(storage_service_t* service,
                                             const uint8_t* data,
                                             size_t length);

/* Appends one newline-terminated message to the active `status.log`. */
esp_err_t storage_service_write_status_message(storage_service_t* service,
                                               const char* message);

/* Copies the active runtime configuration into `config.txt`. */
esp_err_t storage_service_copy_config_snapshot(storage_service_t* service,
                                               const runtime_config_t* config);

/* Closes the open session files if present. */
void storage_service_close_session(storage_service_t* service);

/* Unmounts the storage backend if it is mounted. */
void storage_service_unmount(storage_service_t* service);

/* Returns the active session directory path when one is open. */
const char* storage_service_session_dir(const storage_service_t* service);

/* Returns the current `session.bin` path when available. */
const char* storage_service_binary_path(const storage_service_t* service);

/* Returns the current `status.log` path when available. */
const char* storage_service_status_path(const storage_service_t* service);

/* Returns the current `config.txt` path when available. */
const char* storage_service_config_path(const storage_service_t* service);

/* Returns and clears one pending normalized storage fault. */
bool storage_service_take_pending_fault(storage_service_t* service,
                                        fault_event_t* out_fault);

/* Host-test helper that forces the next write operation to fail. */
esp_err_t storage_service_force_next_write_failure_for_host(
    storage_service_t* service);

#ifdef __cplusplus
}
#endif
