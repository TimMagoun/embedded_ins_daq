#include "storage_service.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char* kDefaultMountPath = "/sdcard";

static esp_err_t copy_text(char* dest, size_t capacity, const char* src) {
  if (dest == NULL || capacity == 0U || src == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  const int written = snprintf(dest, capacity, "%s", src);
  if (written < 0 || (size_t)written >= capacity) {
    return ESP_FAIL;
  }

  return ESP_OK;
}

static void clear_paths(storage_service_t* service) {
  if (service == NULL) {
    return;
  }

  service->session_dir_path[0] = '\0';
  service->binary_path[0] = '\0';
  service->status_path[0] = '\0';
}

static esp_err_t ensure_directory_exists(const char* path) {
  char partial[STORAGE_SERVICE_PATH_CAPACITY];
  struct stat info = {};
  const size_t path_length = path == NULL ? 0U : strlen(path);

  if (path == NULL || path[0] == '\0' || path_length >= sizeof(partial)) {
    return ESP_ERR_INVALID_ARG;
  }

  for (size_t i = 1U; i <= path_length; ++i) {
    if (path[i] != '/' && path[i] != '\0') {
      continue;
    }

    memcpy(partial, path, i);
    partial[i] = '\0';
    if (partial[0] == '\0') {
      continue;
    }

    if (stat(partial, &info) == 0) {
      if (!S_ISDIR(info.st_mode)) {
        return ESP_FAIL;
      }
      continue;
    }

#if defined(_WIN32)
    const int result = _mkdir(partial);
#else
    const int result = mkdir(partial, 0777);
#endif
    if (result != 0 && errno != EEXIST) {
      return ESP_FAIL;
    }
  }

  return ESP_OK;
}

void storage_service_init(storage_service_t* service, int sdmmc_slot) {
  if (service == NULL) {
    return;
  }

  memset(service, 0, sizeof(*service));
  service->sdmmc_slot = sdmmc_slot;
  (void)copy_text(service->base_path, sizeof(service->base_path),
                  kDefaultMountPath);
}

esp_err_t storage_service_init_for_host(storage_service_t* service,
                                        const char* root_path) {
  if (service == NULL || root_path == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  storage_service_init(service, 0);
  service->host_mode = true;
  return copy_text(service->base_path, sizeof(service->base_path), root_path);
}

esp_err_t storage_service_mount(storage_service_t* service) {
  if (service == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (service->mounted) {
    return ESP_OK;
  }

  if (service->host_mode && ensure_directory_exists(service->base_path) == ESP_OK) {
    service->mounted = true;
    return ESP_OK;
  }

  return ESP_ERR_INVALID_STATE;
}

esp_err_t storage_service_open_session(storage_service_t* service,
                                       const session_info_t* session) {
  if (service == NULL || session == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!service->mounted) {
    return ESP_ERR_INVALID_STATE;
  }

  storage_service_close_session(service);
  clear_paths(service);

  if (snprintf(service->session_dir_path, sizeof(service->session_dir_path),
               "%s/session_%06u", service->base_path,
               (unsigned)session->session_id) >=
      (int)sizeof(service->session_dir_path)) {
    return ESP_FAIL;
  }
  if (snprintf(service->binary_path, sizeof(service->binary_path),
               "%s/session.bin", service->session_dir_path) >=
      (int)sizeof(service->binary_path)) {
    return ESP_FAIL;
  }
  if (snprintf(service->status_path, sizeof(service->status_path),
               "%s/status.log", service->session_dir_path) >=
      (int)sizeof(service->status_path)) {
    return ESP_FAIL;
  }

  if (ensure_directory_exists(service->session_dir_path) != ESP_OK) {
    return ESP_FAIL;
  }

  service->binary_file = fopen(service->binary_path, "wb");
  service->status_file = fopen(service->status_path, "w");
  if (service->binary_file == NULL || service->status_file == NULL) {
    storage_service_close_session(service);
    return ESP_FAIL;
  }

  service->session_open = true;
  return ESP_OK;
}

esp_err_t storage_service_write_binary_block(storage_service_t* service,
                                             const uint8_t* data,
                                             size_t length) {
  if (service == NULL || data == NULL || length == 0U) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!service->session_open || service->binary_file == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (fwrite(data, 1U, length, service->binary_file) != length ||
      fflush(service->binary_file) != 0) {
    return ESP_FAIL;
  }

  return ESP_OK;
}

esp_err_t storage_service_write_status_message(storage_service_t* service,
                                               const char* message) {
  if (service == NULL || message == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!service->session_open || service->status_file == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (fprintf(service->status_file, "%s\n", message) < 0 ||
      fflush(service->status_file) != 0) {
    return ESP_FAIL;
  }

  return ESP_OK;
}

void storage_service_close_session(storage_service_t* service) {
  if (service == NULL) {
    return;
  }

  if (service->binary_file != NULL) {
    fclose(service->binary_file);
    service->binary_file = NULL;
  }
  if (service->status_file != NULL) {
    fclose(service->status_file);
    service->status_file = NULL;
  }
  service->session_open = false;
}

void storage_service_unmount(storage_service_t* service) {
  if (service == NULL) {
    return;
  }

  storage_service_close_session(service);
  service->mounted = false;
}

const char* storage_service_session_dir(const storage_service_t* service) {
  return service == NULL ? NULL : service->session_dir_path;
}

const char* storage_service_binary_path(const storage_service_t* service) {
  return service == NULL ? NULL : service->binary_path;
}

const char* storage_service_status_path(const storage_service_t* service) {
  return service == NULL ? NULL : service->status_path;
}
