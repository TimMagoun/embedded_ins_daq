#include <inttypes.h>

#include "esp_err.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "embedded_ins_daq";

void app_main(void) {
  uint32_t flash_size = 0;
  esp_err_t flash_err = esp_flash_get_size(NULL, &flash_size);

  ESP_LOGI(TAG, "Minimal ESP-IDF app starting on %s", CONFIG_IDF_TARGET);
  ESP_LOGI(TAG, "Free heap: %lu bytes",
           (unsigned long)esp_get_free_heap_size());
  if (flash_err == ESP_OK) {
    ESP_LOGI(TAG, "Detected NOR flash: %" PRIu32 " MB",
             flash_size / (1024 * 1024));
  } else {
    ESP_LOGW(TAG, "Failed to query flash size: %s", esp_err_to_name(flash_err));
  }

  if (esp_psram_is_initialized()) {
    ESP_LOGI(TAG, "Detected PSRAM: %u MB",
             (unsigned)(esp_psram_get_size() / (1024 * 1024)));
  } else {
    ESP_LOGW(TAG, "PSRAM is not initialized");
  }

  ESP_LOGI(TAG, "READY: board_smoke");

  while (1) {
    ESP_LOGI(TAG, "Heartbeat");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
