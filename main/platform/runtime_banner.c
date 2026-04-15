#include "runtime_banner.h"

#include <inttypes.h>

#include "clock_service.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "runtime_banner";
static const TickType_t kHealthPeriodTicks = pdMS_TO_TICKS(5000);
static const UBaseType_t kHealthTaskPriority = 1;
static const uint32_t kHealthTaskStackWords = 2048U;

static void runtime_health_task(void* arg) {
  (void)arg;

  while (1) {
    ESP_LOGI(TAG,
             "HEALTH uptime_us=%" PRIu64 " free_heap=%lu stack_high_water=%lu",
             clock_now_us(), (unsigned long)esp_get_free_heap_size(),
             (unsigned long)uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(kHealthPeriodTicks);
  }
}

void runtime_banner_log_startup(void) {
  ESP_LOGI(TAG, "Board: %s", board_name());
  ESP_LOGI(TAG, "Console: %s", board_console_path_name());
  ESP_LOGI(TAG, "Free heap: %lu bytes",
           (unsigned long)esp_get_free_heap_size());
}

void runtime_banner_log_ready(const char* case_name) {
  ESP_LOGI(TAG, "READY: %s", case_name);
}

void runtime_banner_start_health_task(void) {
  xTaskCreatePinnedToCore(runtime_health_task, "task_health_banner",
                          kHealthTaskStackWords, NULL, kHealthTaskPriority,
                          NULL, tskNO_AFFINITY);
}
