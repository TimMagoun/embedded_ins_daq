#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "runtime_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Describes one normalized fault published into the control plane. */
typedef struct {
  fault_code_t code;
  fault_severity_t severity;
} fault_event_t;

/* Holds latched fault counters and aggregate health state. */
typedef struct {
  health_status_t health;
  size_t event_count;
  bool has_fatal_fault;
} fault_manager_t;

/* Resets the fault manager to a healthy baseline. */
void fault_manager_init(fault_manager_t* manager);

/* Publishes a normalized fault event and updates latched health state. */
void fault_manager_publish(fault_manager_t* manager,
                           const fault_event_t* event);

/* Returns the current aggregate health state. */
health_status_t fault_manager_health(const fault_manager_t* manager);

/* Returns the number of fault events seen by the manager. */
size_t fault_manager_event_count(const fault_manager_t* manager);

/* Returns true when a fatal fault has been latched. */
bool fault_manager_has_fatal_fault(const fault_manager_t* manager);

#ifdef __cplusplus
}
#endif
