#pragma once

#include "clock_interface.hpp"

namespace daq {

class HostClockAdapter final : public ClockInterface {
 public:
  /// @brief Creates a deterministic host clock with the provided starting
  /// timestamp.
  /// @param initial_time Initial monotonic microsecond timestamp.
  explicit HostClockAdapter(Timestamp initial_time = 0);

  /// @brief Returns the current deterministic host timestamp.
  /// @return Current monotonic microsecond timestamp.
  Timestamp Now() const override;

  /// @brief Sets the deterministic host timestamp directly.
  /// @param now New monotonic microsecond timestamp.
  void SetNow(Timestamp now);

  /// @brief Advances the deterministic host timestamp by a fixed delta.
  /// @param delta_us Microseconds to add to the current time.
  void AdvanceBy(Timestamp delta_us);

 private:
  Timestamp now_;
};

}  // namespace daq
