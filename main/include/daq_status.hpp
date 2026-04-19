#pragma once

#include <cstdint>

#include "daq_faults.hpp"
#include "daq_types.hpp"

namespace daq {

/// @brief Encodes raw module facts and state-manager lifecycle milestones
/// reported by DAQ.
enum class StatusCode : std::uint8_t {
  kNone = 0,
  kConfigValidationStarted = 1,
  kConfigValidationSucceeded = 2,
  kStorageMounted = 3,
  kStorageRemoved = 4,
  kStartCommandReceived = 5,
  kStopCommandReceived = 6,
  kReady = 7,
  kReadyRevoked = 8,
  kFaultLatched = 9,
  kSessionStarted = 10,
  kSessionStopped = 11,
  kCommandRejected = 12,
};

/// @brief Captures one status publication emitted by a module.
struct StatusEvent {
  ComponentName origin = ComponentName::kNone;
  StatusCode code = StatusCode::kNone;
};

}  // namespace daq
