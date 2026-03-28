#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "platform_iram.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  volatile uint32_t isr_sample_count;
  volatile uint64_t last_isr_timestamp_us;
} clock_service_isr_smoke_state_t;

esp_err_t clock_init(void);
uint64_t clock_now_us(void);
uint64_t PLATFORM_ISR_ATTR clock_now_isr(void);
const char* clock_backend_name(void);
esp_err_t clock_start_isr_smoke(clock_service_isr_smoke_state_t* state);
bool clock_isr_smoke_ready(const clock_service_isr_smoke_state_t* state,
                           uint32_t minimum_samples);

#ifdef __cplusplus
}
#endif
