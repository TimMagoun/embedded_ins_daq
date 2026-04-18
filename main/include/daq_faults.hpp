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

struct FaultCode {
  FaultOrigin origin = FaultOrigin::kNone;
  std::uint16_t detail = 0;
};

}  // namespace daq
