#pragma once

#include <cstdint>

namespace daq {

/// @brief Represents the externally visible DAQ lifecycle state machine.
enum class State : std::uint8_t {
  kInit = 0,
  kReady = 1,
  kRunning = 2,
  kFaulted = 3,
};

/// @brief Identifies which subsystem emitted the latest observable status
/// transition.
enum class ComponentName : std::uint8_t {
  kNone = 0,
  kConfig = 1,
  kStatusManager = 2,
  kDataPlane = 3,
  kStorage = 4,
  kPlatform = 5,
};

/// @brief Distinguishes the record payload families emitted by the acquisition
/// pipeline.
enum class RecordType : std::uint8_t {
  kSession = 0,
  kUartBytes = 1,
  kTrigger = 2,
  kSync = 3,
  kFault = 4,
};

/// @brief Assigns stable identifiers to the logical DAQ input and control
/// ports.
enum class PortId : std::uint8_t {
  kConsole = 0,
  kSensor1 = 1,
  kSensor2 = 2,
  kSensor3 = 3,
  kSensor4 = 4,
};

/// @brief Carries an absolute microsecond timestamp used for ordering and
/// session gating.
using Timestamp = std::uint64_t;

}  // namespace daq
