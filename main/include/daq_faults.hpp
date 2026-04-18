#pragma once

#include <cstdint>

namespace daq {

enum class FaultOrigin : std::uint8_t {
  kNone = 0,
  kConfig = 1,
  kControlPlane = 2,
  kDataPlane = 3,
  kStorage = 4,
  kPlatform = 5,
};

enum class ConfigFaultDetail : std::uint8_t {
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
};

struct FaultCode {
  FaultOrigin origin = FaultOrigin::kNone;
  ConfigFaultDetail detail = ConfigFaultDetail::kNone;
};

constexpr bool operator==(const FaultCode& lhs, const FaultCode& rhs) {
  return lhs.origin == rhs.origin && lhs.detail == rhs.detail;
}

constexpr bool operator!=(const FaultCode& lhs, const FaultCode& rhs) {
  return !(lhs == rhs);
}

constexpr bool is_ok(const FaultCode& fault) { return fault == FaultCode{}; }

}  // namespace daq
