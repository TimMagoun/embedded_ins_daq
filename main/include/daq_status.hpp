#pragma once

#include <cstdint>

#include "daq_faults.hpp"
#include "daq_types.hpp"
#include "status_sink_interface.hpp"

namespace daq {

enum class StatusOrigin : std::uint8_t {
  kNone = 0,
  kConfig = 1,
  kControlPlane = 2,
  kDataPlane = 3,
  kStorage = 4,
  kPlatform = 5,
};

enum class StatusCode : std::uint8_t {
  kNone = 0,
  kConfigValidationStarted = 1,
  kConfigValidationSucceeded = 2,
  kReady = 3,
  kFaultLatched = 4,
};

struct StatusEvent {
  StatusOrigin origin = StatusOrigin::kNone;
  StatusCode code = StatusCode::kNone;
  State state = State::kInit;
  std::uint16_t detail = 0;
};

struct StatusSnapshot {
  State state = State::kInit;
  FaultCode active_fault = {};
  bool session_active = false;
  StatusEvent last_status = {};
};

class StatusFaultHub final : public StatusSinkInterface {
 public:
  void ReportStatus(const StatusEvent& event) override;
  void ReportFault(const FaultCode& fault) override;

  const StatusSnapshot& snapshot() const;

 private:
  StatusSnapshot snapshot_{};
};

}  // namespace daq
