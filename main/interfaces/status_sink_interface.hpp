#pragma once

namespace daq {

struct FaultCode;
struct StatusEvent;

class StatusSinkInterface {
 public:
  virtual ~StatusSinkInterface() = default;

  virtual void ReportStatus(const StatusEvent& event) = 0;
  virtual void ReportFault(const FaultCode& fault) = 0;
};

}  // namespace daq
