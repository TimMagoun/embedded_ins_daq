#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tracks ISR smoke-test progress without exposing it in the main clock API. */
typedef struct {
  volatile uint32_t isr_sample_count;
  volatile uint64_t last_isr_timestamp_us;
} clock_smoke_isr_state_t;

/* Starts the periodic ISR smoke timer used during bring-up verification. */
esp_err_t clock_smoke_start_isr(clock_smoke_isr_state_t* state);

/* Returns true once the ISR smoke test has captured enough samples. */
bool clock_smoke_isr_ready(const clock_smoke_isr_state_t* state,
                           uint32_t minimum_samples);

#ifdef __cplusplus
}
#endif
