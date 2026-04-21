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

/// @brief Distinguishes whether a timing record originated from a generated
/// trigger pulse or an observed sync edge.
enum class TimingEventKind : std::uint8_t {
  kTrigger = 0,
  kSync = 1,
};

/// @brief Describes which signal edge was observed for a timing event.
enum class SignalEdge : std::uint8_t {
  kNone = 0,
  kRising = 1,
  kFalling = 2,
};

/// @brief Immutable descriptor for one captured UART byte chunk.
struct UartChunkRecord {
  PortId port = PortId::kConsole;
  Timestamp first_byte_timestamp = 0;
  Timestamp last_byte_timestamp = 0;
  const std::uint8_t* payload = nullptr;
  std::uint16_t payload_size = 0;
};

/// @brief Immutable descriptor for one captured trigger or sync timing event.
struct TimingEventRecord {
  PortId port = PortId::kConsole;
  Timestamp timestamp = 0;
  TimingEventKind event_kind = TimingEventKind::kTrigger;
  SignalEdge edge = SignalEdge::kNone;
};

/// @brief Immutable view over one storage-mux output block.
struct StorageWriteBlock {
  const std::uint8_t* data = nullptr;
  std::uint16_t size = 0;
  bool flush_after_write = false;
};

}  // namespace daq
