#include "validate_config.hpp"

#include "daq_types.hpp"

namespace {

constexpr daq::FaultCode config_fault(daq::FaultDetail detail) {
  daq::FaultCode fault{};
  fault.origin = daq::ComponentName::kConfig;
  fault.detail = detail;
  return fault;
}

}  // namespace

namespace daq {

FaultCode validate_config(const DaqConfig& config) {
  if (config.enabled_uart_count == 0U) {
    return config_fault(FaultDetail::kNoEnabledUarts);
  }

  if (config.enabled_uart_count > kMaxSupportedUartCount) {
    return config_fault(FaultDetail::kTooManyUarts);
  }

  const auto enabled_uart_mask =
      static_cast<std::uint8_t>(config.enabled_uart_mask & kSupportedUartMask);
  if (enabled_uart_mask != config.enabled_uart_mask ||
      __builtin_popcount(enabled_uart_mask) != config.enabled_uart_count) {
    return config_fault(FaultDetail::kEnabledUartMaskMismatch);
  }

  if (config.uart_chunk_size_bytes > kMaxChunkSizeBytes) {
    return config_fault(FaultDetail::kChunkSizeExceedsBuffer);
  }

  if (config.uart_idle_gap_us < kMinIdleGapUs ||
      config.uart_idle_gap_us > kMaxIdleGapUs) {
    return config_fault(FaultDetail::kIdleGapOutOfRange);
  }

  if (config.record_queue_capacity == 0U ||
      config.writer_queue_capacity == 0U) {
    return config_fault(FaultDetail::kInvalidQueueCapacity);
  }

  if (config.sd_block_size_bytes == 0U ||
      (config.sd_block_size_bytes % kDefaultSdBlockSizeBytes) != 0U) {
    return config_fault(FaultDetail::kInvalidSdBlockSize);
  }

  if ((config.enabled_trigger_mask & enabled_uart_mask) !=
      config.enabled_trigger_mask) {
    return config_fault(FaultDetail::kTriggerMaskWithoutPort);
  }

  if ((config.enabled_sync_mask & enabled_uart_mask) !=
      config.enabled_sync_mask) {
    return config_fault(FaultDetail::kSyncMaskWithoutPort);
  }

  return {};
}

}  // namespace daq
