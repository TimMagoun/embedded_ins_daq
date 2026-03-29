#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Selects whether a port participates in sync or trigger timing flows. */
typedef enum {
  PORT_TIMING_NONE = 0,
  PORT_TIMING_SYNC,
  PORT_TIMING_TRIGGER,
} port_timing_mode_t;

/* Selects which sync-input edges should be captured for a port. */
typedef enum {
  SYNC_EDGE_RISING = 0,
  SYNC_EDGE_FALLING,
  SYNC_EDGE_CHANGE,
} sync_edge_mode_t;

/* Identifies one logical capture port in records and runtime services. */
typedef enum {
  PORT_ID_NONE = 0,
  PORT_ID_1 = 1,
  PORT_ID_2 = 2,
  PORT_ID_3 = 3,
  PORT_ID_4 = 4,
} port_id_t;

/* Summarizes the runtime health state for the active session. */
typedef enum {
  HEALTH_STATUS_OK = 0,
  HEALTH_STATUS_DEGRADED,
  HEALTH_STATUS_FAULTED,
} health_status_t;

/* Normalizes whether a fault can be tolerated or must stop recording. */
typedef enum {
  FAULT_SEVERITY_RECOVERABLE = 0,
  FAULT_SEVERITY_FATAL,
} fault_severity_t;

/* Identifies the coarse fault class used by the control plane. */
typedef enum {
  FAULT_CODE_NONE = 0,
  FAULT_CODE_STORAGE_BACKPRESSURE,
  FAULT_CODE_STORAGE_IO,
  FAULT_CODE_CAPTURE_OVERFLOW,
} fault_code_t;

/* Tracks coarse progress through the runtime session state machine. */
typedef enum {
  SESSION_BOOT = 0,
  SESSION_CONFIG_INVALID,
  SESSION_READY,
  SESSION_STARTING,
  SESSION_RECORDING,
  SESSION_STOPPING,
  SESSION_FAULTED,
} session_state_t;

#ifdef __cplusplus
}
#endif
