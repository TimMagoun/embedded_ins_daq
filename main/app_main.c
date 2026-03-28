#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "board_profile.h"
#include "clock_probe.h"
#include "clock_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform_iram.h"
#include "runtime_banner.h"

static const char* TAG = "embedded_ins_daq";
static const uint32_t kClockSmokeTaskDelayMs = 50U;
static const uint32_t kClockSmokeMaxAttempts = 20U;

static PLATFORM_INTERNAL_RAM clock_service_isr_smoke_state_t s_isr_smoke_state;

static uint64_t app_clock_probe_read(void* ctx) {
  (void)ctx;
  return clock_now_us();
}

static bool run_clock_monotonicity_smoke(void) {
  uint32_t attempt;
  uint64_t last_sample = 0;

  if (!clock_probe_monotonic(app_clock_probe_read, NULL, 64U, &last_sample)) {
    ESP_LOGE(TAG, "Clock monotonicity probe failed in task context");
    return false;
  }

  ESP_LOGI(TAG, "Clock monotonicity probe passed: last_sample_us=%llu",
           (unsigned long long)last_sample);

  for (attempt = 0; attempt < kClockSmokeMaxAttempts; ++attempt) {
    if (clock_isr_smoke_ready(&s_isr_smoke_state, 3U)) {
      ESP_LOGI(TAG, "Clock ISR smoke passed: isr_samples=%lu last_isr_us=%llu",
               (unsigned long)s_isr_smoke_state.isr_sample_count,
               (unsigned long long)s_isr_smoke_state.last_isr_timestamp_us);
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(kClockSmokeTaskDelayMs));
  }

  ESP_LOGE(TAG, "Clock ISR smoke timed out: isr_samples=%lu",
           (unsigned long)s_isr_smoke_state.isr_sample_count);
  return false;
}

void app_main(void) {
  const board_profile_t* board = board_profile_active();
  board_profile_validation_result_t validation = {0};

  ESP_ERROR_CHECK(clock_init());
  if (!board_profile_validate(board, &validation)) {
    ESP_LOGE(TAG, "Board profile validation failed: %s",
             board_profile_validation_message(validation.code));
    ESP_LOGE(TAG, "Failure details: port_index=%d gpio_a=%d gpio_b=%d",
             validation.port_index, validation.gpio_a, validation.gpio_b);
    abort();
  }

  runtime_banner_log_startup(board);
  ESP_ERROR_CHECK(clock_start_isr_smoke(&s_isr_smoke_state));

  if (run_clock_monotonicity_smoke()) {
    runtime_banner_log_ready("clock_monotonicity");
  } else {
    ESP_LOGE(TAG, "Clock monotonicity smoke failed");
  }

  runtime_banner_start_health_task();
  runtime_banner_log_ready("platform_smoke");
}
