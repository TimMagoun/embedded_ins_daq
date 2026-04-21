#pragma once

#include <cstdint>

#include "daq_types.hpp"

namespace daq {

/// @brief Computes the elapsed microseconds between two ordered timestamps.
/// @param start Earlier timestamp.
/// @param end Later timestamp.
/// @return Non-negative elapsed microseconds.
constexpr Timestamp ElapsedMicros(Timestamp start, Timestamp end) {
  return end < start ? 0U : (end - start);
}

/// @brief Returns whether the UART idle gap threshold has been reached.
/// @param last_activity Timestamp of the most recent byte in the chunk.
/// @param observed_at Current monotonic timestamp.
/// @param idle_gap_us Configured idle-gap threshold in microseconds.
/// @return `true` when the idle gap has expired.
constexpr bool HasIdleGapExpired(Timestamp last_activity, Timestamp observed_at,
                                 std::uint32_t idle_gap_us) {
  return ElapsedMicros(last_activity, observed_at) >= idle_gap_us;
}

}  // namespace daq
