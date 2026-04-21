#include "esp32_clock_adapter.hpp"

#include "driver/gptimer.h"
#include "esp_err.h"
#include "esp_log.h"

namespace {
constexpr char kTag[] = "esp32_clock";
}

namespace daq {

Esp32ClockAdapter::~Esp32ClockAdapter() { ResetTimer(); }

/// @brief Allocates and starts the shared GPTimer at 1 MHz resolution.
/// @return `ESP_OK` on success or the first ESP-IDF error encountered.
/// @note The adapter owns no policy beyond timer setup; all timestamp
/// interpretation stays in the monotonic-time core.
esp_err_t Esp32ClockAdapter::Initialize() {
  if (timer_ != nullptr) {
    ESP_LOGW(kTag, "GPTimer initialize called twice; reusing existing timer");
    return ESP_OK;
  }

  ESP_LOGD(kTag, "Allocating GPTimer at 1 MHz");

  gptimer_config_t config{};
  config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  config.direction = GPTIMER_COUNT_UP;
  config.resolution_hz = 1'000'000U;

  esp_err_t error = gptimer_new_timer(&config, &timer_);
  if (error != ESP_OK) {
    ESP_LOGE(kTag, "gptimer_new_timer failed: %s", esp_err_to_name(error));
    return error;
  }

  error = gptimer_enable(timer_);
  if (error != ESP_OK) {
    ESP_LOGE(kTag, "gptimer_enable failed: %s", esp_err_to_name(error));
    ResetTimer();
    return error;
  }

  error = gptimer_start(timer_);
  if (error != ESP_OK) {
    ESP_LOGE(kTag, "gptimer_start failed: %s", esp_err_to_name(error));
    ResetTimer();
    return error;
  }

  ESP_LOGI(kTag, "GPTimer started at 1 MHz");
  return ESP_OK;
}

/// @brief Reads the current raw GPTimer counter in microseconds.
/// @return Current monotonic timestamp when the adapter is initialized and the
/// driver read succeeds; otherwise `0`.
/// @note Invalid-state reads are logged explicitly because the clock contract
/// treats them as internal faults rather than recoverable absence.
Timestamp Esp32ClockAdapter::Now() const {
  if (timer_ == nullptr) {
    ESP_LOGE(kTag, "Now called before GPTimer initialization");
    return 0;
  }

  std::uint64_t count = 0;
  const esp_err_t error = gptimer_get_raw_count(timer_, &count);
  if (error != ESP_OK) {
    ESP_LOGE(kTag, "gptimer_get_raw_count failed: %s", esp_err_to_name(error));
    return 0;
  }

  return count;
}

/// @brief Releases a partially initialized GPTimer so initialization can be
/// retried cleanly.
/// @note Cleanup errors are ignored because the original initialization
/// failure remains the authoritative error for the caller.
void Esp32ClockAdapter::ResetTimer() {
  if (timer_ == nullptr) {
    return;
  }

  ESP_LOGD(kTag, "Releasing GPTimer");
  static_cast<void>(gptimer_disable(timer_));
  static_cast<void>(gptimer_del_timer(timer_));
  timer_ = nullptr;
}

}  // namespace daq
