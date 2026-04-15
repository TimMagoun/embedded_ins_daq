#pragma once

#include "fault_manager.h"
#include "runtime_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Owns the boot/start/record/fault state transitions for one session. */
typedef struct {
  session_state_t state;
  fault_manager_t fault_manager;
  runtime_config_error_t last_config_error;
  bool storage_ready;
  bool config_loaded;
} session_controller_t;

/* Initializes the session controller in boot state. */
void session_controller_init(session_controller_t* controller);

/* Marks storage as ready for automatic session startup. */
esp_err_t session_controller_mark_storage_ready(
    session_controller_t* controller);

/* Validates and latches the runtime config during boot. */
esp_err_t session_controller_mark_config_loaded(
    session_controller_t* controller, const runtime_config_t* config);

/* Requests session start using the provided runtime configuration. */
bool session_controller_request_start(session_controller_t* controller,
                                      const runtime_config_t* config);

/* Marks the session as actively recording after startup completes. */
esp_err_t session_controller_mark_recording(session_controller_t* controller);

/* Publishes a runtime fault into the session state machine. */
void session_controller_publish_fault(session_controller_t* controller,
                                      const fault_event_t* event);

/* Returns the current session state. */
session_state_t session_controller_state(
    const session_controller_t* controller);

/* Returns the last runtime configuration error observed by the controller. */
runtime_config_error_t session_controller_last_config_error(
    const session_controller_t* controller);

#ifdef __cplusplus
}
#endif
