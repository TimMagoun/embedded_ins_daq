#pragma once

#include "daq_config.hpp"
#include "daq_faults.hpp"

namespace daq {

FaultCode validate_config(const DaqConfig& config);

}  // namespace daq
