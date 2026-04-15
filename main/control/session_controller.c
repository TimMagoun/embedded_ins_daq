#include "session_controller.h"

void session_controller_init(session_controller_t* controller) {
  if (controller == NULL) {
    return;
  }

  controller->state = SESSION_BOOT;
  controller->last_config_error = RUNTIME_CONFIG_ERROR_NONE;
  controller->storage_ready = false;
  controller->config_loaded = false;
  fault_manager_init(&controller->fault_manager);
}

static void update_ready_state(session_controller_t* controller) {
  if (controller == NULL) {
    return;
  }

  if (controller->state == SESSION_BOOT && controller->storage_ready &&
      controller->config_loaded) {
    controller->state = SESSION_READY;
  }
}

esp_err_t session_controller_mark_storage_ready(
    session_controller_t* controller) {
  if (controller == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (controller->state != SESSION_BOOT) {
    return ESP_ERR_INVALID_STATE;
  }

  controller->storage_ready = true;
  update_ready_state(controller);
  return ESP_OK;
}

esp_err_t session_controller_mark_config_loaded(
    session_controller_t* controller, const runtime_config_t* config) {
  if (controller == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (controller->state != SESSION_BOOT && controller->state != SESSION_READY) {
    return ESP_ERR_INVALID_STATE;
  }

  if (runtime_config_validate(config, &controller->last_config_error) !=
      ESP_OK) {
    controller->state = SESSION_CONFIG_INVALID;
    controller->config_loaded = false;
    return ESP_ERR_INVALID_ARG;
  }

  controller->config_loaded = true;
  update_ready_state(controller);
  return ESP_OK;
}

bool session_controller_request_start(session_controller_t* controller,
                                      const runtime_config_t* config) {
  if (controller == NULL) {
    return false;
  }

  if (controller->state != SESSION_BOOT && controller->state != SESSION_READY) {
    return false;
  }

  if (runtime_config_validate(config, &controller->last_config_error) !=
      ESP_OK) {
    controller->state = SESSION_CONFIG_INVALID;
    return false;
  }

  controller->state = SESSION_STARTING;
  return true;
}

esp_err_t session_controller_mark_recording(session_controller_t* controller) {
  if (controller == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (controller->state != SESSION_STARTING) {
    return ESP_ERR_INVALID_STATE;
  }

  controller->state = SESSION_RECORDING;
  return ESP_OK;
}

void session_controller_publish_fault(session_controller_t* controller,
                                      const fault_event_t* event) {
  if (controller == NULL) {
    return;
  }

  fault_manager_publish(&controller->fault_manager, event);
  if (fault_manager_has_fatal_fault(&controller->fault_manager)) {
    controller->state = SESSION_FAULTED;
  }
}

session_state_t session_controller_state(
    const session_controller_t* controller) {
  if (controller == NULL) {
    return SESSION_FAULTED;
  }

  return controller->state;
}

runtime_config_error_t session_controller_last_config_error(
    const session_controller_t* controller) {
  if (controller == NULL) {
    return RUNTIME_CONFIG_ERROR_NULL_CONFIG;
  }

  return controller->last_config_error;
}
