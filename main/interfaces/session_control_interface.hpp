#pragma once

#include "daq_faults.hpp"
#include "daq_types.hpp"

namespace daq {

class SessionControlInterface {
 public:
  virtual ~SessionControlInterface() = default;

  /// @brief Arms the downstream acquisition path after configuration succeeds.
  virtual void Arm() = 0;

  /// @brief Starts a new session using the accepted session boundary
  /// timestamp.
  /// @param session_start Accepted session boundary timestamp.
  virtual void StartSession(const Timestamp& session_start) = 0;

  /// @brief Stops the active session and returns the downstream path to its
  /// idle state.
  virtual void StopSession() = 0;

  /// @brief Forces an immediate shutdown after a fault.
  /// @param fault Latched fault that explains the shutdown reason.
  virtual void FaultShutdown(const FaultCode& fault) = 0;
};

}  // namespace daq
