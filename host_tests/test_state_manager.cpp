#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

#include "daq_faults.hpp"
#include "daq_status.hpp"
#include "session_control_interface.hpp"
#include "state_manager.hpp"
#include "status_sink_interface.hpp"

namespace {

using daq::ComponentName;
using daq::FaultCode;
using daq::FaultDetail;
using daq::State;
using daq::StateManager;
using daq::StatusCode;
using daq::StatusEvent;
using daq::Timestamp;

constexpr Timestamp kStartA{100U};
constexpr Timestamp kStartB{200U};
constexpr FaultCode kFault{ComponentName::kStorage,
                           FaultDetail::kStorageWriteFailed};

struct SessionCall {
  enum class Type {
    kArm,
    kStartSession,
    kStopSession,
    kFaultShutdown,
  };

  Type type = Type::kArm;
  Timestamp session_start{};
  FaultCode fault{};
};

class FakeSessionControl final : public daq::SessionControlInterface {
 public:
  void Arm() override { calls.push_back(SessionCall{SessionCall::Type::kArm}); }

  void StartSession(const Timestamp& session_start) override {
    SessionCall call{};
    call.type = SessionCall::Type::kStartSession;
    call.session_start = session_start;
    calls.push_back(call);
  }

  void StopSession() override {
    calls.push_back(SessionCall{SessionCall::Type::kStopSession});
  }

  void FaultShutdown(const FaultCode& fault) override {
    SessionCall call{};
    call.type = SessionCall::Type::kFaultShutdown;
    call.fault = fault;
    calls.push_back(call);
  }

  std::vector<SessionCall> calls;
};

class FakeStatusSink final : public daq::StatusSinkInterface {
 public:
  void ReportStatus(const StatusEvent& event) override {
    statuses.push_back(event);
  }

  void ReportFault(const FaultCode& fault) override { faults.push_back(fault); }

  std::vector<StatusEvent> statuses;
  std::vector<FaultCode> faults;
};

constexpr StatusEvent make_status(const ComponentName origin,
                                  const StatusCode code) {
  StatusEvent event{};
  event.origin = origin;
  event.code = code;
  return event;
}

template <typename T, typename = void>
struct has_session_active : std::false_type {};

template <typename T>
struct has_session_active<
    T, std::void_t<decltype(std::declval<const T&>().session_active())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_session_start : std::false_type {};

template <typename T>
struct has_session_start<
    T, std::void_t<decltype(std::declval<const T&>().session_start())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_accepts_record_timestamp : std::false_type {};

template <typename T>
struct has_accepts_record_timestamp<
    T, std::void_t<decltype(std::declval<const T&>().AcceptsRecordTimestamp(
           std::declval<const Timestamp&>()))>> : std::true_type {};

TEST(StateManager,
     ShouldNotExposeSessionWindowQueryApiGivenStorageMuxOwnsWriteAdmission) {
  EXPECT_FALSE(has_session_active<StateManager>::value);
  EXPECT_FALSE(has_session_start<StateManager>::value);
  EXPECT_FALSE(has_accepts_record_timestamp<StateManager>::value);
}

TEST(StateManager,
     ShouldDispatchArmAndReportReadyGivenConfigAndStorageReadyStatuses) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);

  manager.OnStatusEvent(make_status(ComponentName::kConfig,
                                    StatusCode::kConfigValidationSucceeded));
  EXPECT_TRUE(session_control.calls.empty());
  ASSERT_EQ(status_sink.statuses.size(), 1U);
  EXPECT_EQ(status_sink.statuses[0].code,
            StatusCode::kConfigValidationSucceeded);

  manager.OnStatusEvent(
      make_status(ComponentName::kStorage, StatusCode::kStorageMounted));

  ASSERT_EQ(session_control.calls.size(), 1U);
  EXPECT_EQ(session_control.calls[0].type, SessionCall::Type::kArm);
  ASSERT_EQ(status_sink.statuses.size(), 3U);
  EXPECT_EQ(status_sink.statuses[0].code,
            StatusCode::kConfigValidationSucceeded);
  EXPECT_EQ(status_sink.statuses[1].code, StatusCode::kStorageMounted);
  EXPECT_EQ(status_sink.statuses[2].code, StatusCode::kReady);
  EXPECT_EQ(manager.state(), State::kReady);
}

TEST(StateManager,
     ShouldDispatchStartAndStopCommandsGivenNominalSessionLifecycleStatuses) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);
  manager.OnStatusEvent(make_status(ComponentName::kConfig,
                                    StatusCode::kConfigValidationSucceeded));
  manager.OnStatusEvent(
      make_status(ComponentName::kStorage, StatusCode::kStorageMounted));

  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStartCommandReceived),
      kStartA);
  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStopCommandReceived));

  ASSERT_EQ(session_control.calls.size(), 3U);
  EXPECT_EQ(session_control.calls[1].type, SessionCall::Type::kStartSession);
  EXPECT_EQ(session_control.calls[1].session_start, kStartA);
  EXPECT_EQ(session_control.calls[2].type, SessionCall::Type::kStopSession);
  ASSERT_EQ(status_sink.statuses.size(), 7U);
  EXPECT_EQ(status_sink.statuses[5].code, StatusCode::kStopCommandReceived);
  EXPECT_EQ(status_sink.statuses[6].code, StatusCode::kSessionStopped);
  EXPECT_EQ(status_sink.statuses[3].code, StatusCode::kStartCommandReceived);
  EXPECT_EQ(status_sink.statuses[4].code, StatusCode::kSessionStarted);
  EXPECT_EQ(manager.state(), State::kReady);
}

TEST(StateManager,
     ShouldReportRejectedStatusWithoutSessionCommandGivenStartBeforeReady) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);

  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStartCommandReceived),
      kStartA);

  EXPECT_TRUE(session_control.calls.empty());
  ASSERT_EQ(status_sink.statuses.size(), 2U);
  EXPECT_EQ(status_sink.statuses[0].code, StatusCode::kStartCommandReceived);
  EXPECT_EQ(status_sink.statuses[1].code, StatusCode::kCommandRejected);
  EXPECT_TRUE(status_sink.faults.empty());
}

TEST(StateManager,
     ShouldShutdownModulesAndReportFaultAndStatusGivenAcceptedFault) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);
  manager.OnStatusEvent(make_status(ComponentName::kConfig,
                                    StatusCode::kConfigValidationSucceeded));
  manager.OnStatusEvent(
      make_status(ComponentName::kStorage, StatusCode::kStorageMounted));
  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStartCommandReceived),
      kStartA);

  manager.OnFault(kFault);

  ASSERT_EQ(session_control.calls.size(), 3U);
  EXPECT_EQ(session_control.calls[2].type, SessionCall::Type::kFaultShutdown);
  EXPECT_EQ(session_control.calls[2].fault, kFault);
  ASSERT_EQ(status_sink.faults.size(), 1U);
  EXPECT_EQ(status_sink.faults[0], kFault);
  ASSERT_EQ(status_sink.statuses.size(), 6U);
  EXPECT_EQ(status_sink.statuses[5].code, StatusCode::kFaultLatched);
  EXPECT_EQ(manager.active_fault(), kFault);
}

TEST(StateManager, ShouldResetSessionBoundaryGivenRepeatedStartStopCycles) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);
  manager.OnStatusEvent(make_status(ComponentName::kConfig,
                                    StatusCode::kConfigValidationSucceeded));
  manager.OnStatusEvent(
      make_status(ComponentName::kStorage, StatusCode::kStorageMounted));
  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStartCommandReceived),
      kStartA);
  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStopCommandReceived));
  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStartCommandReceived),
      kStartB);

  ASSERT_EQ(session_control.calls.size(), 4U);
  EXPECT_EQ(session_control.calls[3].type, SessionCall::Type::kStartSession);
  EXPECT_EQ(session_control.calls[3].session_start, kStartB);
  EXPECT_EQ(manager.state(), State::kRunning);
}

TEST(StateManager,
     ShouldRejectLaterEventsAndPreserveFirstFaultGivenFaultAlreadyLatched) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);

  manager.OnFault(kFault);
  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStartCommandReceived),
      kStartA);
  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStopCommandReceived));
  const FaultCode later_fault{ComponentName::kDataPlane,
                              FaultDetail::kQueueOverflow};
  manager.OnFault(later_fault);

  ASSERT_EQ(session_control.calls.size(), 1U);
  EXPECT_EQ(session_control.calls[0].type, SessionCall::Type::kFaultShutdown);
  ASSERT_EQ(status_sink.faults.size(), 1U);
  EXPECT_EQ(status_sink.faults[0], kFault);
  ASSERT_EQ(status_sink.statuses.size(), 6U);
  EXPECT_EQ(status_sink.statuses[0].code, StatusCode::kFaultLatched);
  EXPECT_EQ(status_sink.statuses[1].code, StatusCode::kStartCommandReceived);
  EXPECT_EQ(status_sink.statuses[2].code, StatusCode::kCommandRejected);
  EXPECT_EQ(status_sink.statuses[3].code, StatusCode::kStopCommandReceived);
  EXPECT_EQ(status_sink.statuses[4].code, StatusCode::kCommandRejected);
  EXPECT_EQ(status_sink.statuses[5].code, StatusCode::kCommandRejected);
  EXPECT_EQ(manager.state(), State::kFaulted);
  EXPECT_EQ(manager.active_fault(), kFault);
}

TEST(StateManager,
     ShouldRevokeReadyWithoutStartingSessionGivenStorageRemovedWhileReady) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);

  manager.OnStatusEvent(make_status(ComponentName::kConfig,
                                    StatusCode::kConfigValidationSucceeded));
  manager.OnStatusEvent(
      make_status(ComponentName::kStorage, StatusCode::kStorageMounted));
  manager.OnStatusEvent(
      make_status(ComponentName::kStorage, StatusCode::kStorageRemoved));

  ASSERT_EQ(session_control.calls.size(), 1U);
  EXPECT_EQ(session_control.calls[0].type, SessionCall::Type::kArm);
  ASSERT_EQ(status_sink.statuses.size(), 5U);
  EXPECT_EQ(status_sink.statuses[2].code, StatusCode::kReady);
  EXPECT_EQ(status_sink.statuses[3].code, StatusCode::kStorageRemoved);
  EXPECT_EQ(status_sink.statuses[4].code, StatusCode::kReadyRevoked);
  EXPECT_EQ(manager.state(), State::kInit);
}

TEST(StateManager,
     ShouldIgnoreUnknownStatusWithoutTransitionGivenUnrecognizedModuleFact) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);

  manager.OnStatusEvent(make_status(ComponentName::kPlatform,
                                    StatusCode::kConfigValidationStarted));

  EXPECT_TRUE(session_control.calls.empty());
  ASSERT_EQ(status_sink.statuses.size(), 1U);
  EXPECT_EQ(status_sink.statuses[0].code, StatusCode::kConfigValidationStarted);
  EXPECT_EQ(manager.state(), State::kInit);
}

TEST(StateManager, ShouldFaultAndShutdownGivenStorageRemovedWhileRunning) {
  FakeSessionControl session_control;
  FakeStatusSink status_sink;
  StateManager manager(session_control, status_sink);

  manager.OnStatusEvent(make_status(ComponentName::kConfig,
                                    StatusCode::kConfigValidationSucceeded));
  manager.OnStatusEvent(
      make_status(ComponentName::kStorage, StatusCode::kStorageMounted));
  manager.OnStatusEvent(
      make_status(ComponentName::kPlatform, StatusCode::kStartCommandReceived),
      kStartA);
  manager.OnStatusEvent(
      make_status(ComponentName::kStorage, StatusCode::kStorageRemoved));

  ASSERT_EQ(session_control.calls.size(), 3U);
  EXPECT_EQ(session_control.calls[2].type, SessionCall::Type::kFaultShutdown);
  EXPECT_EQ(session_control.calls[2].fault.origin, ComponentName::kStorage);
  EXPECT_EQ(session_control.calls[2].fault.detail, FaultDetail::kPlatformError);
  ASSERT_EQ(status_sink.faults.size(), 1U);
  EXPECT_EQ(status_sink.faults[0].origin, ComponentName::kStorage);
  EXPECT_EQ(status_sink.faults[0].detail, FaultDetail::kPlatformError);
  EXPECT_EQ(status_sink.statuses.back().code, StatusCode::kFaultLatched);
  EXPECT_EQ(manager.state(), State::kFaulted);
}

}  // namespace
