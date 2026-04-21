#pragma once

#include "clock_interface.hpp"
#include "driver/gptimer.h"  // IWYU pragma: keep
#include "esp_err.h"

namespace daq {

class Esp32ClockAdapter final : public ClockInterface {
 public:
  Esp32ClockAdapter() = default;
  Esp32ClockAdapter(const Esp32ClockAdapter&) = delete;
  Esp32ClockAdapter& operator=(const Esp32ClockAdapter&) = delete;
  Esp32ClockAdapter(Esp32ClockAdapter&&) = delete;
  Esp32ClockAdapter& operator=(Esp32ClockAdapter&&) = delete;
  ~Esp32ClockAdapter() override;

  /// @brief Configures and starts a 1 MHz GPTimer used as the canonical time
  /// source.
  /// @return `ESP_OK` on success or the first ESP-IDF error encountered.
  esp_err_t Initialize();

  /// @brief Reads the current GPTimer count in microseconds.
  /// @return Current monotonic microsecond timestamp, or `0` if the adapter is
  /// uninitialized or the timer read fails.
  Timestamp Now() const override;

 private:
  /// @brief Tears down a partially initialized timer handle after setup
  /// failure.
  void ResetTimer();

  gptimer_handle_t timer_ = nullptr;
};

}  // namespace daq
