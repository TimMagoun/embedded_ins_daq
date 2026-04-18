#pragma once

#include <cstddef>
#include <cstdint>

namespace daq {

constexpr std::uint8_t kMaxSupportedUartCount = 4U;
constexpr std::uint8_t kSupportedUartMask = 0x0FU;
constexpr std::size_t kMaxChunkSizeBytes = 256U;
constexpr std::uint32_t kMinIdleGapUs = 50U;
constexpr std::uint32_t kMaxIdleGapUs = 100000U;
constexpr std::size_t kDefaultSdBlockSizeBytes = 512U;

struct DaqConfig {
  std::uint8_t enabled_uart_count = 2U;
  std::uint8_t enabled_uart_mask = 0x03U;
  std::size_t uart_chunk_size_bytes = 128U;
  std::uint32_t uart_idle_gap_us = 1000U;
  std::size_t record_queue_capacity = 8U;
  std::size_t writer_queue_capacity = 4U;
  std::size_t sd_block_size_bytes = kDefaultSdBlockSizeBytes;
  std::uint8_t enabled_trigger_mask = 0x01U;
  std::uint8_t enabled_sync_mask = 0x01U;
};

inline constexpr DaqConfig kDefaultConfig{};

}  // namespace daq
