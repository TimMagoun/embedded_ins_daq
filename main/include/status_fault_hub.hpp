#pragma once

#include "daq_status.hpp"
#include "status_sink_interface.hpp"

namespace daq {

class StatusFaultHub final : public StatusSinkInterface {
 public:
  void ReportStatus(const StatusEvent& event) override;
  void ReportFault(const FaultCode& fault) override;

  const StatusSnapshot& snapshot() const;

 private:
  StatusSnapshot snapshot_{};
};

}  // namespace daq
