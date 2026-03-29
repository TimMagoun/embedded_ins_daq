#include "fault_manager.h"

void fault_manager_init(fault_manager_t* manager) {
  if (manager == NULL) {
    return;
  }

  manager->health = HEALTH_STATUS_OK;
  manager->event_count = 0;
  manager->has_fatal_fault = false;
}

void fault_manager_publish(fault_manager_t* manager,
                           const fault_event_t* event) {
  if (manager == NULL || event == NULL) {
    return;
  }

  manager->event_count += 1U;
  if (event->severity == FAULT_SEVERITY_FATAL) {
    manager->has_fatal_fault = true;
    manager->health = HEALTH_STATUS_FAULTED;
    return;
  }

  if (manager->health == HEALTH_STATUS_OK) {
    manager->health = HEALTH_STATUS_DEGRADED;
  }
}

health_status_t fault_manager_health(const fault_manager_t* manager) {
  if (manager == NULL) {
    return HEALTH_STATUS_FAULTED;
  }

  return manager->health;
}

size_t fault_manager_event_count(const fault_manager_t* manager) {
  if (manager == NULL) {
    return 0;
  }

  return manager->event_count;
}

bool fault_manager_has_fatal_fault(const fault_manager_t* manager) {
  if (manager == NULL) {
    return true;
  }

  return manager->has_fatal_fault;
}
