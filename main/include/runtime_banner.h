#pragma once

#include "board_ports.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Logs startup identity, memory, and fixed board wiring details. */
void runtime_banner_log_startup(void);
/* Emits a standard READY banner for automation and smoke tests. */
void runtime_banner_log_ready(const char* case_name);
/* Starts the low-priority periodic health logger task. */
void runtime_banner_start_health_task(void);

#ifdef __cplusplus
}
#endif
