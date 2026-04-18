#include "status_fault_hub.hpp"

namespace daq {

void StatusFaultHub::ReportStatus(const StatusEvent& event) {
  snapshot_.last_status = event;
  snapshot_.state = event.state;
  snapshot_.session_active = event.state == State::kRunning;
}

void StatusFaultHub::ReportFault(const FaultCode& fault) {
  if (is_ok(snapshot_.active_fault) && !is_ok(fault)) {
    snapshot_.active_fault = fault;
  }

  snapshot_.state = State::kFaulted;
  snapshot_.session_active = false;
}

const StatusSnapshot& StatusFaultHub::snapshot() const { return snapshot_; }

}  // namespace daq
