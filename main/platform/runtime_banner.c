#include "runtime_banner.h"

#include <inttypes.h>

#include "clock_service.h"
#include "esp_app_desc.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_psram.h"
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

void runtime_banner_log_startup(const board_profile_t* profile) {
  const esp_app_desc_t* app_desc = esp_app_get_description();
  uint32_t flash_size = 0;
  esp_err_t flash_err = esp_flash_get_size(NULL, &flash_size);
  int i;

  ESP_LOGI(TAG, "Firmware version: %s", app_desc->version);
  ESP_LOGI(TAG, "Build ID: %s", EMBEDDED_INS_DAQ_BUILD_ID);
  ESP_LOGI(TAG, "Active board profile: %s", profile->profile_name);
  ESP_LOGI(TAG, "Console path: %s (UART%d TX=%d RX=%d)",
           profile->console_path_name, profile->console_uart_controller,
           profile->console_tx_gpio, profile->console_rx_gpio);
  ESP_LOGI(TAG, "Clock backend: %s", clock_backend_name());
  ESP_LOGI(TAG, "Target: %s", CONFIG_IDF_TARGET);
  ESP_LOGI(TAG, "Free heap: %lu bytes",
           (unsigned long)esp_get_free_heap_size());

  if (flash_err == ESP_OK) {
    ESP_LOGI(TAG, "Detected NOR flash: %" PRIu32 " MB",
             flash_size / (1024U * 1024U));
  } else {
    ESP_LOGW(TAG, "Failed to query flash size: %s", esp_err_to_name(flash_err));
  }

  if (esp_psram_is_initialized()) {
    ESP_LOGI(TAG, "Detected PSRAM: %u MB",
             (unsigned)(esp_psram_get_size() / (1024U * 1024U)));
  } else {
    ESP_LOGW(TAG, "PSRAM is not initialized");
  }

  ESP_LOGI(TAG, "SD interface: onboard TF slot via SDMMC_HOST_SLOT_%d",
           profile->sdmmc_slot);
  for (i = 0; i < BOARD_PORT_COUNT; ++i) {
    const board_port_profile_t* port = &profile->ports[i];
    if (!port->enabled) {
      ESP_LOGI(TAG, "%s disabled: UART/SYNC mapping reserved for later stage",
               port->name);
      continue;
    }

    ESP_LOGI(TAG, "%s provisional map: UART%d TX=%d RX=%d SYNC=%d", port->name,
             port->uart_controller, port->tx_gpio, port->rx_gpio,
             port->sync_gpio);
  }
}

void runtime_banner_log_ready(const char* case_name) {
  ESP_LOGI(TAG, "READY: %s", case_name);
}

void runtime_banner_start_health_task(void) {
  xTaskCreatePinnedToCore(runtime_health_task, "task_health_banner",
                          kHealthTaskStackWords, NULL, kHealthTaskPriority,
                          NULL, tskNO_AFFINITY);
}
