#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "binary_log_pipeline.h"
}

namespace {

record_buffer_t MakeRecord(uint16_t type, uint64_t timestamp_us,
                           uint8_t payload_byte) {
  record_buffer_t record = {};
  binary_record_header_t header = {};
  uint8_t payload[2] = {payload_byte, static_cast<uint8_t>(payload_byte + 1U)};

  header.record_type = type;
  header.record_version = RECORD_FORMAT_VERSION;
  header.payload_length = sizeof(payload);
  header.timestamp_us = timestamp_us;
  header.source_id = payload_byte;

  std::memcpy(record.bytes, &header, sizeof(header));
  std::memcpy(record.bytes + sizeof(header), payload, sizeof(payload));
  record.length = sizeof(header) + sizeof(payload);

  return record;
}

template <typename T>
T CopyStructAt(const std::vector<uint8_t>& bytes, size_t offset) {
  T value = {};
  EXPECT_LE(offset + sizeof(T), bytes.size());
  if (offset + sizeof(T) > bytes.size()) {
    return value;
  }
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

TEST(BinaryLogPipelineTest, PreservesAppendOrderAcrossFlushBoundaries) {
  std::array<uint8_t, 256> staging = {};
  std::array<uint8_t, 64> flush_buffer = {};
  binary_log_pipeline_t pipeline = {};
  std::vector<uint8_t> drained;
  size_t bytes_written = 0;

  binary_log_pipeline_init(&pipeline, staging.data(), staging.size());

  const record_buffer_t first = MakeRecord(RECORD_TYPE_SESSION_START, 11U, 1U);
  const record_buffer_t second = MakeRecord(RECORD_TYPE_FAULT_EVENT, 22U, 2U);
  const record_buffer_t third = MakeRecord(RECORD_TYPE_UART_DATA, 33U, 3U);

  ASSERT_EQ(binary_log_pipeline_append(&pipeline, &first, nullptr), ESP_OK);
  ASSERT_EQ(binary_log_pipeline_append(&pipeline, &second, nullptr), ESP_OK);
  ASSERT_EQ(binary_log_pipeline_append(&pipeline, &third, nullptr), ESP_OK);

  while (binary_log_pipeline_pending_bytes(&pipeline) > 0U) {
    ASSERT_EQ(binary_log_pipeline_flush(&pipeline, flush_buffer.data(),
                                        flush_buffer.size(), &bytes_written),
              ESP_OK);
    drained.insert(drained.end(), flush_buffer.begin(),
                   flush_buffer.begin() + bytes_written);
  }

  ASSERT_EQ(drained.size(), first.length + second.length + third.length);

  const binary_record_header_t header_1 =
      CopyStructAt<binary_record_header_t>(drained, 0);
  const binary_record_header_t header_2 =
      CopyStructAt<binary_record_header_t>(drained, first.length);
  const binary_record_header_t header_3 = CopyStructAt<binary_record_header_t>(
      drained, first.length + second.length);

  EXPECT_EQ(header_1.record_type, RECORD_TYPE_SESSION_START);
  EXPECT_EQ(header_1.timestamp_us, 11U);
  EXPECT_EQ(header_2.record_type, RECORD_TYPE_FAULT_EVENT);
  EXPECT_EQ(header_2.timestamp_us, 22U);
  EXPECT_EQ(header_3.record_type, RECORD_TYPE_UART_DATA);
  EXPECT_EQ(header_3.timestamp_us, 33U);
}

TEST(BinaryLogPipelineTest, EmitsOverflowFaultWhenStagingIsFull) {
  std::array<uint8_t, 48> staging = {};
  binary_log_pipeline_t pipeline = {};
  fault_event_t overflow_fault = {};

  binary_log_pipeline_init(&pipeline, staging.data(), staging.size());

  const record_buffer_t record = MakeRecord(RECORD_TYPE_UART_DATA, 44U, 9U);
  ASSERT_EQ(binary_log_pipeline_append(&pipeline, &record, nullptr), ESP_OK);

  EXPECT_EQ(binary_log_pipeline_append(&pipeline, &record, &overflow_fault),
            ESP_ERR_NO_MEM);
  EXPECT_EQ(overflow_fault.code, FAULT_CODE_STORAGE_BACKPRESSURE);
  EXPECT_EQ(overflow_fault.severity, FAULT_SEVERITY_RECOVERABLE);
}

}  // namespace
