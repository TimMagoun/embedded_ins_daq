#pragma once

#include <cstdint>

namespace daq {

enum class State : std::uint8_t {
  kInit = 0,
  kReady = 1,
  kRunning = 2,
  kFaulted = 3,
};

enum class RecordType : std::uint8_t {
  kSession = 0,
  kUartBytes = 1,
  kTrigger = 2,
  kSync = 3,
  kFault = 4,
};

enum class PortId : std::uint8_t {
  kConsole = 0,
  kSensor1 = 1,
  kSensor2 = 2,
  kTrigger = 3,
  kSync = 4,
};

struct Timestamp {
  std::uint64_t microseconds = 0;
};

}  // namespace daq
