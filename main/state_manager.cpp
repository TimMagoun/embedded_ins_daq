#include "state_manager.hpp"

namespace daq {

/// @brief Binds the pure state-machine core to the injected side-effect
/// adapters.
/// @param session_control Adapter that executes accepted lifecycle commands.
/// @param status_sink Adapter that publishes status and fault events.
/// @note The wrapper keeps sequencing observable in host tests by separating
/// pure transitions from adapter side effects.
StateManager::StateManager(SessionControlInterface& session_control,
                           StatusSinkInterface& status_sink)
    : session_control_(&session_control), status_sink_(&status_sink) {}

/// @brief Mirrors one raw module status event into the sink, updates any
/// readiness prerequisites, and applies the resulting lifecycle transition.
/// @param event Raw module event emitted by another subsystem.
/// @param observed_at Timestamp associated with the event when needed for the
/// accepted session boundary.
/// @note Logging the raw event before interpretation preserves the exact event
/// order for postmortem reconstruction while keeping lifecycle authority local
/// to the manager.
void StateManager::OnStatusEvent(const StatusEvent& event,
                                 const Timestamp observed_at) {
  status_sink_->ReportStatus(event);

  if (state_ == State::kFaulted) {
    ReportRejectedCommand();
    return;
  }

  const bool recognized = UpdatePrerequisites(event);

  switch (event.code) {
    case StatusCode::kStartCommandReceived:
      if (state_ != State::kReady) {
        ReportRejectedCommand();
        return;
      }

      state_ = State::kRunning;
      session_control_->StartSession(observed_at);
      ReportAcceptedStatus(StatusCode::kSessionStarted);
      return;

    case StatusCode::kStopCommandReceived:
      if (state_ != State::kRunning) {
        ReportRejectedCommand();
        return;
      }

      state_ = State::kReady;
      session_control_->StopSession();
      ReportAcceptedStatus(StatusCode::kSessionStopped);
      return;

    default:
      if (!recognized) {
        return;
      }

      if (state_ == State::kRunning && !storage_mounted_) {
        LatchFaultAndShutdown(
            FaultCode{ComponentName::kStorage, FaultDetail::kPlatformError});
        ReportAcceptedStatus(StatusCode::kFaultLatched);
        return;
      }

      if (state_ == State::kReady && (!config_valid_ || !storage_mounted_)) {
        RevokeReadyIfNeeded();
        return;
      }

      if (state_ == State::kInit) {
        EnterReadyIfEligible();
      }
      return;
  }
}

void StateManager::OnFault(const FaultCode& fault) {
  if (state_ == State::kFaulted || is_ok(fault)) {
    ReportRejectedCommand();
    return;
  }

  LatchFaultAndShutdown(fault);
  ReportAcceptedStatus(StatusCode::kFaultLatched);
}

State StateManager::state() const { return state_; }

FaultCode StateManager::active_fault() const { return active_fault_; }

/// @brief Publishes the accepted lifecycle event for the current transition.
/// @param accepted_code Status code to emit when the transition is accepted.
/// @note The state manager is the sole source of lifecycle status publications.
void StateManager::ReportAcceptedStatus(const StatusCode accepted_code) {
  StatusEvent status{};
  status.origin = ComponentName::kStatusManager;
  status.code = accepted_code;
  status_sink_->ReportStatus(status);
}

/// @brief Publishes a rejected-command status without dispatching adapters.
/// @note The mechanism deliberately avoids adapter side effects so invalid
/// lifecycle requests cannot perturb the running session path.
void StateManager::ReportRejectedCommand() {
  StatusEvent status{};
  status.origin = ComponentName::kStatusManager;
  status.code = StatusCode::kCommandRejected;
  status_sink_->ReportStatus(status);
}

/// @brief Applies recognized non-fault status facts to the readiness
/// prerequisite set.
/// @param event Raw module status event to interpret.
/// @return `true` when the manager recognized the event as affecting readiness.
bool StateManager::UpdatePrerequisites(const StatusEvent& event) {
  switch (event.origin) {
    case ComponentName::kConfig:
      if (event.code == StatusCode::kConfigValidationSucceeded) {
        config_valid_ = true;
        return true;
      }
      return false;

    case ComponentName::kStorage:
      if (event.code == StatusCode::kStorageMounted) {
        storage_mounted_ = true;
        return true;
      }

      if (event.code == StatusCode::kStorageRemoved) {
        storage_mounted_ = false;
        return true;
      }
      return false;

    default:
      return false;
  }
}

/// @brief Moves the manager from `init` to `ready` once all required
/// readiness facts are present.
/// @note The manager arms downstream modules only once per accepted entry into
/// `ready`.
void StateManager::EnterReadyIfEligible() {
  if (state_ != State::kInit || !config_valid_ || !storage_mounted_) {
    return;
  }

  state_ = State::kReady;
  session_control_->Arm();
  ReportAcceptedStatus(StatusCode::kReady);
}

/// @brief Revokes `ready` when a required prerequisite is lost.
/// @note The v1 control path drops back to `init` so later start commands stay
/// rejected until readiness is re-established.
void StateManager::RevokeReadyIfNeeded() {
  if (state_ != State::kReady || (config_valid_ && storage_mounted_)) {
    return;
  }

  state_ = State::kInit;
  ReportAcceptedStatus(StatusCode::kReadyRevoked);
}

/// @brief Applies the one-way fault transition and notifies downstream
/// adapters.
/// @param fault Non-OK fault to latch as authoritative.
/// @note The first fault wins; later faults never call this helper because
/// `OnFault()` rejects once faulted.
void StateManager::LatchFaultAndShutdown(const FaultCode& fault) {
  active_fault_ = fault;
  state_ = State::kFaulted;
  session_control_->FaultShutdown(active_fault_);
  status_sink_->ReportFault(active_fault_);
}

}  // namespace daq
