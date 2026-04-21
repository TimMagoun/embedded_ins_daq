#pragma once

#include "daq_types.hpp"

namespace daq {

class ClockInterface {
 public:
  virtual ~ClockInterface() = default;

  /// @brief Reads the current monotonic time in microseconds.
  /// @return Monotonic microsecond timestamp. Implementations must log invalid
  /// states or read failures and return `0` when no valid timestamp can be
  /// produced.
  virtual Timestamp Now() const = 0;
};

}  // namespace daq
