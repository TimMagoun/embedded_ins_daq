#pragma once

#include "daq_faults.hpp"
#include "daq_status.hpp"
#include "daq_types.hpp"
#include "session_control_interface.hpp"
#include "status_sink_interface.hpp"

namespace daq {

class StateManager {
 public:
  /// @brief Binds the state machine to the side-effect adapters that execute
  /// accepted commands.
  /// @param session_control Adapter that performs accepted lifecycle commands.
  /// @param status_sink Adapter that publishes accepted and rejected status
  /// events.
  StateManager(SessionControlInterface& session_control,
               StatusSinkInterface& status_sink);

  /// @brief Records one externally emitted status fact and applies any
  /// resulting lifecycle transition.
  /// @param event Raw module status event to log and interpret.
  /// @param observed_at Timestamp associated with the event when the code needs
  /// a session boundary, otherwise zero.
  /// @note The manager mirrors the raw event to the status sink before
  /// interpreting it so diagnostics can reconstruct the exact event timeline.
  void OnStatusEvent(const StatusEvent& event, Timestamp observed_at = {});

  /// @brief Latches the first non-OK fault and requests downstream shutdown.
  /// @param fault Non-OK fault code to latch.
  /// @note Later faults are rejected so the original fault remains
  /// authoritative.
  void OnFault(const FaultCode& fault);

  /// @brief Returns the current lifecycle state after the latest accepted
  /// transition.
  /// @return The current DAQ lifecycle state.
  State state() const;

  /// @brief Returns the first latched non-OK fault.
  /// @return `FaultCode{}` when no fault has been accepted.
  FaultCode active_fault() const;

 private:
  /// @brief Publishes an accepted lifecycle status event.
  /// @param accepted_code Status code to publish for the accepted transition.
  void ReportAcceptedStatus(StatusCode accepted_code);

  /// @brief Publishes a rejected-command status while preserving the current
  /// lifecycle snapshot.
  void ReportRejectedCommand();

  /// @brief Updates readiness prerequisites for raw module status facts.
  /// @param event Raw module status event to interpret.
  /// @return `true` when the fact was recognized by the state manager.
  bool UpdatePrerequisites(const StatusEvent& event);

  /// @brief Promotes the manager into `ready` when all readiness facts are
  /// satisfied.
  void EnterReadyIfEligible();

  /// @brief Revokes `ready` when a required prerequisite is lost.
  void RevokeReadyIfNeeded();

  /// @brief Latches the first fault, clears transient session state, and
  /// requests downstream shutdown.
  /// @param fault Non-OK fault code to latch when not already faulted.
  void LatchFaultAndShutdown(const FaultCode& fault);

  SessionControlInterface* session_control_;
  StatusSinkInterface* status_sink_;
  State state_ = State::kInit;
  FaultCode active_fault_{};
  bool config_valid_ = false;
  bool storage_mounted_ = false;
};

}  // namespace daq
