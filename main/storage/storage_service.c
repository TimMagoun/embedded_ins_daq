#include "storage_service.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char* kDefaultMountPath = "/sdcard";

static void clear_paths(storage_service_t* service) {
  if (service == NULL) {
    return;
  }

  service->session_dir_path[0] = '\0';
  service->binary_path[0] = '\0';
  service->status_path[0] = '\0';
  service->config_path[0] = '\0';
}

static esp_err_t set_fault(storage_service_t* service, fault_code_t code,
                           fault_severity_t severity, esp_err_t err) {
  if (service != NULL) {
    service->pending_fault_valid = true;
    service->pending_fault.code = code;
    service->pending_fault.severity = severity;
  }
  return err;
}

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

static esp_err_t write_format_line(FILE* file, const char* format, ...) {
  va_list args;

  if (file == NULL || format == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  va_start(args, format);
  const int written = vfprintf(file, format, args);
  va_end(args);
  if (written < 0) {
    return ESP_FAIL;
  }

  if (fflush(file) != 0) {
    return ESP_FAIL;
  }

  return ESP_OK;
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

static esp_err_t fail_if_injected(storage_service_t* service) {
  if (service == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!service->fail_next_write_for_host) {
    return ESP_OK;
  }

  service->fail_next_write_for_host = false;
  return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                   ESP_FAIL);
}

static esp_err_t format_config_snapshot(FILE* file,
                                        const runtime_config_t* config) {
  if (file == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (write_format_line(file, "port_count=%u\n",
                        (unsigned)config->port_count) != ESP_OK) {
    return ESP_FAIL;
  }

  for (size_t i = 0; i < config->port_count; ++i) {
    const runtime_port_config_t* port = &config->ports[i];
    if (write_format_line(
            file,
            "port%u.enabled=%u\nport%u.uart_port=%d\nport%u.baud_rate=%d\n"
            "port%u.timing_mode=%u\nport%u.sync_edge_mode=%u\n"
            "port%u.enable_sync_input=%u\n"
            "port%u.trigger_period_us=%u\nport%u.trigger_pulse_width_us=%u\n",
            (unsigned)(i + 1U), port->enabled ? 1U : 0U, (unsigned)(i + 1U),
            port->uart_port, (unsigned)(i + 1U), port->baud_rate,
            (unsigned)(i + 1U), (unsigned)port->timing_mode, (unsigned)(i + 1U),
            (unsigned)port->sync_edge_mode, (unsigned)(i + 1U),
            port->enable_sync_input ? 1U : 0U, (unsigned)(i + 1U),
            (unsigned)port->trigger_period_us, (unsigned)(i + 1U),
            (unsigned)port->trigger_pulse_width_us) != ESP_OK) {
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

  if (service->host_mode) {
    const esp_err_t dir_err = ensure_directory_exists(service->base_path);
    if (dir_err != ESP_OK) {
      return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                       dir_err);
    }
    service->mounted = true;
    return ESP_OK;
  }

  return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                   ESP_ERR_INVALID_STATE);
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
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
  }
  if (snprintf(service->binary_path, sizeof(service->binary_path),
               "%s/session.bin", service->session_dir_path) >=
      (int)sizeof(service->binary_path)) {
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
  }
  if (snprintf(service->status_path, sizeof(service->status_path),
               "%s/status.log", service->session_dir_path) >=
      (int)sizeof(service->status_path)) {
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
  }
  if (snprintf(service->config_path, sizeof(service->config_path),
               "%s/config.txt", service->session_dir_path) >=
      (int)sizeof(service->config_path)) {
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
  }

  if (ensure_directory_exists(service->session_dir_path) != ESP_OK) {
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
  }

  service->binary_file = fopen(service->binary_path, "wb");
  service->status_file = fopen(service->status_path, "w");
  if (service->binary_file == NULL || service->status_file == NULL) {
    storage_service_close_session(service);
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
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

  const esp_err_t injected = fail_if_injected(service);
  if (injected != ESP_OK) {
    return injected;
  }

  if (fwrite(data, 1U, length, service->binary_file) != length ||
      fflush(service->binary_file) != 0) {
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
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

  const esp_err_t injected = fail_if_injected(service);
  if (injected != ESP_OK) {
    return injected;
  }

  if (fprintf(service->status_file, "%s\n", message) < 0 ||
      fflush(service->status_file) != 0) {
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
  }

  return ESP_OK;
}

esp_err_t storage_service_copy_config_snapshot(storage_service_t* service,
                                               const runtime_config_t* config) {
  FILE* file = NULL;

  if (service == NULL || config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!service->session_open) {
    return ESP_ERR_INVALID_STATE;
  }

  const esp_err_t injected = fail_if_injected(service);
  if (injected != ESP_OK) {
    return injected;
  }

  file = fopen(service->config_path, "w");
  if (file == NULL) {
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL,
                     ESP_FAIL);
  }

  const esp_err_t err = format_config_snapshot(file, config);
  fclose(file);
  if (err != ESP_OK) {
    return set_fault(service, FAULT_CODE_STORAGE_IO, FAULT_SEVERITY_FATAL, err);
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

const char* storage_service_config_path(const storage_service_t* service) {
  return service == NULL ? NULL : service->config_path;
}

bool storage_service_take_pending_fault(storage_service_t* service,
                                        fault_event_t* out_fault) {
  if (service == NULL || out_fault == NULL || !service->pending_fault_valid) {
    return false;
  }

  *out_fault = service->pending_fault;
  service->pending_fault_valid = false;
  service->pending_fault = (fault_event_t){0};
  return true;
}

esp_err_t storage_service_force_next_write_failure_for_host(
    storage_service_t* service) {
  if (service == NULL || !service->host_mode) {
    return ESP_ERR_INVALID_ARG;
  }

  service->fail_next_write_for_host = true;
  return ESP_OK;
}
