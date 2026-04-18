#pragma once

#include "daq_faults.hpp"
#include "daq_types.hpp"

namespace daq {

struct StatusSnapshot {
  State state = State::kBooting;
  FaultCode active_fault = {};
  bool session_active = false;
};

}  // namespace daq
