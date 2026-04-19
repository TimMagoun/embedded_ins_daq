#include <gtest/gtest.h>

#include "daq_config.hpp"
#include "daq_faults.hpp"
#include "validate_config.hpp"

namespace {

using daq::ComponentName;
using daq::DaqConfig;
using daq::FaultCode;
using daq::FaultDetail;
using daq::validate_config;

constexpr FaultCode kOk{};

void expect_fault(FaultCode actual, FaultDetail expected_detail) {
  EXPECT_EQ(actual.origin, ComponentName::kConfig);
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

TEST(ValidateConfig,
     ShouldReportEnabledUartMaskMismatchGivenMaskPopulationDiffersFromCount) {
  DaqConfig config = daq::kDefaultConfig;
  config.enabled_uart_count = 1U;
  config.enabled_uart_mask = 0x03U;
  expect_fault(validate_config(config), FaultDetail::kEnabledUartMaskMismatch);
}

TEST(ValidateConfig, ShouldReportInvalidQueueCapacityGivenZeroQueueCapacity) {
  DaqConfig config = daq::kDefaultConfig;
  config.record_queue_capacity = 0U;
  expect_fault(validate_config(config), FaultDetail::kInvalidQueueCapacity);
}

TEST(ValidateConfig,
     ShouldReportInvalidSdBlockSizeGivenBlockSizeOutsideSupportedMultiple) {
  DaqConfig config = daq::kDefaultConfig;
  config.sd_block_size_bytes = daq::kDefaultSdBlockSizeBytes + 1U;
  expect_fault(validate_config(config), FaultDetail::kInvalidSdBlockSize);
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

TEST(ValidateConfig,
     ShouldReportTriggerMaskWithoutPortGivenTriggerMaskTargetsDisabledPort) {
  DaqConfig config = daq::kDefaultConfig;
  config.enabled_uart_count = 1U;
  config.enabled_uart_mask = 0x01U;
  config.enabled_trigger_mask = 0x02U;
  expect_fault(validate_config(config), FaultDetail::kTriggerMaskWithoutPort);
}

}  // namespace
