#include <gtest/gtest.h>

#include "daq_config.hpp"
#include "daq_faults.hpp"
#include "daq_status.hpp"
#include "status_fault_hub.hpp"
#include "validate_config.hpp"

namespace {

using daq::DaqConfig;
using daq::FaultCode;
using daq::FaultDetail;
using daq::FaultOrigin;
using daq::StatusCode;
using daq::StatusEvent;
using daq::StatusFaultHub;
using daq::StatusOrigin;
using daq::validate_config;

constexpr FaultCode kOk{};

void expect_fault(FaultCode actual, FaultDetail expected_detail) {
  EXPECT_EQ(actual.origin, FaultOrigin::kConfig);
  EXPECT_EQ(actual.detail, expected_detail);
}

TEST(ValidateConfig, ShouldReturnOkGivenDefaultConfiguration) {
  const FaultCode result = validate_config(daq::kDefaultConfig);
  EXPECT_EQ(result, kOk);
}

TEST(ValidateConfig, ShouldReportTooManyUartsGivenCountAboveSupportedMaximum) {
  DaqConfig config = daq::kDefaultConfig;
  config.enabled_uart_count = daq::kMaxSupportedUartCount + 1U;
  expect_fault(validate_config(config), FaultDetail::kTooManyUarts);
}

TEST(ValidateConfig, ShouldReportNoEnabledUartsGivenZeroEnabledPorts) {
  DaqConfig config = daq::kDefaultConfig;
  config.enabled_uart_count = 0U;
  config.enabled_uart_mask = 0U;
  expect_fault(validate_config(config), FaultDetail::kNoEnabledUarts);
}

TEST(ValidateConfig,
     ShouldReportChunkSizeExceedsBufferGivenChunkSizeAboveFixedBufferLimit) {
  DaqConfig config = daq::kDefaultConfig;
  config.uart_chunk_size_bytes = daq::kMaxChunkSizeBytes + 1U;
  expect_fault(validate_config(config), FaultDetail::kChunkSizeExceedsBuffer);
}

TEST(ValidateConfig, ShouldReturnOkGivenChunkSizeAtFixedBufferLimit) {
  DaqConfig config = daq::kDefaultConfig;
  config.uart_chunk_size_bytes = daq::kMaxChunkSizeBytes;
  EXPECT_EQ(validate_config(config), kOk);
}

TEST(ValidateConfig, ShouldReturnOkGivenIdleGapAtMinimumBoundary) {
  DaqConfig config = daq::kDefaultConfig;
  config.uart_idle_gap_us = daq::kMinIdleGapUs;
  EXPECT_EQ(validate_config(config), kOk);
}

TEST(ValidateConfig, ShouldReturnOkGivenIdleGapAtMaximumBoundary) {
  DaqConfig config = daq::kDefaultConfig;
  config.uart_idle_gap_us = daq::kMaxIdleGapUs;
  EXPECT_EQ(validate_config(config), kOk);
}

TEST(ValidateConfig, ShouldReportInvalidQueueCapacityGivenZeroQueueCapacity) {
  DaqConfig config = daq::kDefaultConfig;
  config.record_queue_capacity = 0U;
  expect_fault(validate_config(config), FaultDetail::kInvalidQueueCapacity);
}

TEST(ValidateConfig,
     ShouldReportIdleGapOutOfRangeGivenIdleGapBelowMinimumBoundary) {
  DaqConfig config = daq::kDefaultConfig;
  config.uart_idle_gap_us = daq::kMinIdleGapUs - 1U;
  expect_fault(validate_config(config), FaultDetail::kIdleGapOutOfRange);
}

TEST(ValidateConfig,
     ShouldReportSyncMaskWithoutPortGivenSyncMaskTargetsDisabledPort) {
  DaqConfig config = daq::kDefaultConfig;
  config.enabled_uart_count = 1U;
  config.enabled_uart_mask = 0x01U;
  config.enabled_sync_mask = 0x02U;
  expect_fault(validate_config(config), FaultDetail::kSyncMaskWithoutPort);
}

TEST(StatusFaultHub,
     ShouldPreserveFirstFailureReasonGivenMultipleFaultReports) {
  StatusFaultHub hub;

  StatusEvent started{};
  started.origin = StatusOrigin::kConfig;
  started.code = StatusCode::kConfigValidationStarted;
  started.state = daq::State::kInit;
  started.detail = 11U;
  hub.ReportStatus(started);

  FaultCode first_fault{};
  first_fault.origin = FaultOrigin::kConfig;
  first_fault.detail = FaultDetail::kChunkSizeExceedsBuffer;
  hub.ReportFault(first_fault);

  FaultCode second_fault{};
  second_fault.origin = FaultOrigin::kConfig;
  second_fault.detail = FaultDetail::kInvalidQueueCapacity;
  hub.ReportFault(second_fault);

  const daq::StatusSnapshot snapshot = hub.snapshot();

  EXPECT_EQ(snapshot.active_fault.origin, FaultOrigin::kConfig);
  EXPECT_EQ(snapshot.active_fault.detail, FaultDetail::kChunkSizeExceedsBuffer);
  EXPECT_EQ(snapshot.last_status.origin, StatusOrigin::kConfig);
  EXPECT_EQ(snapshot.last_status.code, StatusCode::kConfigValidationStarted);
}

TEST(StatusSinkInterface, ShouldAcceptStatusFaultHubGivenStatusSinkReference) {
  StatusFaultHub hub;
  daq::StatusSinkInterface& sink = hub;
  StatusEvent ready{};
  ready.origin = StatusOrigin::kControlPlane;
  ready.code = StatusCode::kReady;
  ready.state = daq::State::kReady;
  sink.ReportStatus(ready);

  EXPECT_EQ(hub.snapshot().last_status.code, StatusCode::kReady);
}

}  // namespace
