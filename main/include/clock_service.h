#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the active clock backend and logs the selected source. */
esp_err_t clock_init(void);

/* Returns the current timestamp in microseconds in task context. */
uint64_t clock_now_us(void);

/* Returns the current timestamp in microseconds from ISR-safe context. */
uint64_t clock_now_isr(void);

/* Returns a stable short name for the selected clock backend. */
const char* clock_backend_name(void);

#ifdef __cplusplus
}
#endif
