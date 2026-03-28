#include "clock_service.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "platform_iram.h"

static const char* TAG = "clock_service";

esp_err_t clock_init(void) {
  int64_t now_us = esp_timer_get_time();
  if (now_us < 0) {
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Clock backend ready: %s", clock_backend_name());
  return ESP_OK;
}

uint64_t clock_now_us(void) { return (uint64_t)esp_timer_get_time(); }

uint64_t PLATFORM_ISR_ATTR clock_now_isr(void) {
  return (uint64_t)esp_timer_get_time();
}

const char* clock_backend_name(void) { return "esp_timer_systimer_us"; }
