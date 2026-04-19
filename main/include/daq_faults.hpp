#pragma once

#include <cstdint>

#include "daq_types.hpp"

namespace daq {

enum class FaultDetail : std::uint8_t {
  kNone = 0,
  kNoEnabledUarts = 1,
  kTooManyUarts = 2,
  kEnabledUartMaskMismatch = 3,
  kChunkSizeExceedsBuffer = 4,
  kIdleGapOutOfRange = 5,
  kInvalidQueueCapacity = 6,
  kInvalidSdBlockSize = 7,
  kTriggerMaskWithoutPort = 8,
  kSyncMaskWithoutPort = 9,
  kInvalidStateTransition = 10,
  kQueueOverflow = 11,
  kStorageWriteFailed = 12,
  kPlatformError = 13,
};

struct FaultCode {
  ComponentName origin = ComponentName::kNone;
  FaultDetail detail = FaultDetail::kNone;
};

constexpr bool operator==(const FaultCode& lhs, const FaultCode& rhs) {
  return lhs.origin == rhs.origin && lhs.detail == rhs.detail;
}

constexpr bool operator!=(const FaultCode& lhs, const FaultCode& rhs) {
  return !(lhs == rhs);
}

constexpr bool is_ok(const FaultCode& fault) { return fault == FaultCode{}; }

}  // namespace daq
