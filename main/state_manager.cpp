#include "state_manager.hpp"

namespace {

constexpr std::uint16_t kConfigCommandDetail = 1U;
constexpr std::uint16_t kStartCommandDetail = 2U;
constexpr std::uint16_t kStopCommandDetail = 3U;
constexpr std::uint16_t kFaultCommandDetail = 4U;

}  // namespace

namespace daq {

StateManager::StateManager(SessionControlInterface& session_control,
                           StatusSinkInterface& status_sink)
    : session_control_(session_control), status_sink_(status_sink) {}

void StateManager::OnConfigResult(const FaultCode& config_result) {
  HandleResult(
      core_.OnConfigResult(config_result),
      is_ok(config_result) ? StatusCode::kReady : StatusCode::kFaultLatched,
      kConfigCommandDetail);
}

void StateManager::Start(const Timestamp& session_start) {
  HandleResult(core_.Start(session_start), StatusCode::kSessionStarted,
               kStartCommandDetail);
}

void StateManager::Stop() {
  HandleResult(core_.Stop(), StatusCode::kSessionStopped, kStopCommandDetail);
}

void StateManager::Fault(const FaultCode& fault) {
  HandleResult(core_.Fault(fault), StatusCode::kFaultLatched,
               kFaultCommandDetail);
}

State StateManager::state() const { return core_.state(); }

const StateManagerSnapshot& StateManager::snapshot() const {
  return core_.snapshot();
}

bool StateManager::AcceptsRecordTimestamp(const Timestamp& timestamp) const {
  return core_.AcceptsRecordTimestamp(timestamp);
}

void StateManager::HandleResult(const StateManagerResult& result,
                                const StatusCode accepted_code,
                                const std::uint16_t detail) {
  if (result.disposition == StateManagerEventDisposition::kRejected) {
    ReportRejectedCommand(detail);
    return;
  }

  if (result.command.has_value()) {
    switch (result.command->type) {
      case StateManagerCommandType::kArm:
        session_control_.Arm();
        break;
      case StateManagerCommandType::kStartSession:
        session_control_.StartSession(result.command->session_start);
        break;
      case StateManagerCommandType::kStopSession:
        session_control_.StopSession();
        break;
      case StateManagerCommandType::kFaultShutdown:
        session_control_.FaultShutdown(result.command->fault);
        status_sink_.ReportFault(result.command->fault);
        return;
    }
  }

  StatusEvent status{};
  status.origin = StatusOrigin::kControlPlane;
  status.code = accepted_code;
  status.state = core_.state();
  status.detail = detail;
  status_sink_.ReportStatus(status);
}

void StateManager::ReportRejectedCommand(const std::uint16_t detail) {
  StatusEvent status{};
  status.origin = StatusOrigin::kControlPlane;
  status.code = StatusCode::kCommandRejected;
  status.state = core_.state();
  status.detail = detail;
  status_sink_.ReportStatus(status);
}

}  // namespace daq
