#pragma once

#include <cstdint>

#include "daq_faults.hpp"
#include "daq_types.hpp"

namespace daq {

/// @brief Encodes the externally visible lifecycle and fault milestones
/// reported by DAQ.
enum class StatusCode : std::uint8_t {
  kNone = 0,
  kConfigValidationStarted = 1,
  kConfigValidationSucceeded = 2,
  kReady = 3,
  kFaultLatched = 4,
  kSessionStarted = 5,
  kSessionStopped = 6,
  kCommandRejected = 7,
};

/// @brief Captures one status publication with the state snapshot observers can
/// act on.
struct StatusEvent {
  ComponentName origin = ComponentName::kNone;
  StatusCode code = StatusCode::kNone;
  State state = State::kInit;
  std::uint16_t detail = 0;
};

/// @brief Summarizes the currently latched state, fault, and latest reported
/// status event.
struct StatusSnapshot {
  State state = State::kInit;
  FaultCode active_fault = {};
  bool session_active = false;
  StatusEvent last_status = {};
};

}  // namespace daq
