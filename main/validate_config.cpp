#include "validate_config.hpp"

namespace {

constexpr std::uint8_t popcount_u8(std::uint8_t value) {
  std::uint8_t count = 0;
  while (value != 0U) {
    count = static_cast<std::uint8_t>(count + (value & 0x01U));
    value = static_cast<std::uint8_t>(value >> 1U);
  }
  return count;
}

constexpr daq::FaultCode config_fault(daq::ConfigFaultDetail detail) {
  daq::FaultCode fault{};
  fault.origin = daq::FaultOrigin::kConfig;
  fault.detail = detail;
  return fault;
}

}  // namespace

namespace daq {

FaultCode validate_config(const DaqConfig& config) {
  if (config.enabled_uart_count == 0U) {
    return config_fault(ConfigFaultDetail::kNoEnabledUarts);
  }

  if (config.enabled_uart_count > kMaxSupportedUartCount) {
    return config_fault(ConfigFaultDetail::kTooManyUarts);
  }

  const std::uint8_t enabled_uart_mask =
      static_cast<std::uint8_t>(config.enabled_uart_mask & kSupportedUartMask);
  if (enabled_uart_mask != config.enabled_uart_mask ||
      popcount_u8(enabled_uart_mask) != config.enabled_uart_count) {
    return config_fault(ConfigFaultDetail::kEnabledUartMaskMismatch);
  }

  if (config.uart_chunk_size_bytes > kMaxChunkSizeBytes) {
    return config_fault(ConfigFaultDetail::kChunkSizeExceedsBuffer);
  }

  if (config.uart_idle_gap_us < kMinIdleGapUs ||
      config.uart_idle_gap_us > kMaxIdleGapUs) {
    return config_fault(ConfigFaultDetail::kIdleGapOutOfRange);
  }

  if (config.record_queue_capacity == 0U ||
      config.writer_queue_capacity == 0U) {
    return config_fault(ConfigFaultDetail::kInvalidQueueCapacity);
  }

  if (config.sd_block_size_bytes == 0U ||
      (config.sd_block_size_bytes % kDefaultSdBlockSizeBytes) != 0U) {
    return config_fault(ConfigFaultDetail::kInvalidSdBlockSize);
  }

  if ((config.enabled_trigger_mask & enabled_uart_mask) !=
      config.enabled_trigger_mask) {
    return config_fault(ConfigFaultDetail::kTriggerMaskWithoutPort);
  }

  if ((config.enabled_sync_mask & enabled_uart_mask) !=
      config.enabled_sync_mask) {
    return config_fault(ConfigFaultDetail::kSyncMaskWithoutPort);
  }

  return {};
}

}  // namespace daq
