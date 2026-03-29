#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"

extern "C" {
#include "binary_log_pipeline.h"
#include "uart_capture_service.h"
}

namespace {

template <typename T>
T CopyStructAt(const uint8_t* bytes, size_t length, size_t offset) {
  T value = {};
  EXPECT_LE(offset + sizeof(T), length);
  if (offset + sizeof(T) > length) {
    return value;
  }
  std::memcpy(&value, bytes + offset, sizeof(T));
  return value;
}

TEST(UartCaptureServiceTest, PublishesChunkTimestampFromFirstByteArrival) {
  std::array<uint8_t, 512> pipeline_storage = {};
  std::array<uint8_t, 32> ring_storage = {};
  binary_log_pipeline_t pipeline = {};
  uart_capture_service_t service = {};
  record_buffer_t record = {};
  const std::array<uint8_t, 3> first = {0x10, 0x11, 0x12};
  const std::array<uint8_t, 2> second = {0x13, 0x14};

  binary_log_pipeline_init(&pipeline, pipeline_storage.data(),
                           pipeline_storage.size());
  ASSERT_EQ(uart_capture_service_init(&service, PORT_ID_1, ring_storage.data(),
                                      ring_storage.size(), &pipeline),
            ESP_OK);

  ASSERT_EQ(uart_capture_service_on_rx_bytes(&service, PORT_ID_1, 1000U,
                                             first.data(), first.size()),
            ESP_OK);
  ASSERT_EQ(uart_capture_service_on_rx_bytes(&service, PORT_ID_1, 1200U,
                                             second.data(), second.size()),
            ESP_OK);
  ASSERT_EQ(uart_capture_service_publish_pending(&service, &record), ESP_OK);

  const binary_record_header_t header =
      CopyStructAt<binary_record_header_t>(record.bytes, record.length, 0U);
  const uart_data_record_payload_prefix_t prefix =
      CopyStructAt<uart_data_record_payload_prefix_t>(
          record.bytes, record.length, sizeof(header));

  EXPECT_EQ(header.record_type, RECORD_TYPE_UART_DATA);
  EXPECT_EQ(header.timestamp_us, 1000U);
  EXPECT_EQ(header.source_id, PORT_ID_1);
  EXPECT_EQ(prefix.data_length, first.size() + second.size());
  EXPECT_EQ(0, std::memcmp(record.bytes + sizeof(header) + sizeof(prefix),
                           first.data(), first.size()));
  EXPECT_EQ(0, std::memcmp(record.bytes + sizeof(header) + sizeof(prefix) +
                               first.size(),
                           second.data(), second.size()));
}

TEST(UartCaptureServiceTest, PreservesBytesAcrossWraparound) {
  std::array<uint8_t, 512> pipeline_storage = {};
  std::array<uint8_t, 8> ring_storage = {};
  binary_log_pipeline_t pipeline = {};
  uart_capture_service_t service = {};
  record_buffer_t record = {};
  const std::array<uint8_t, 6> first = {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5};
  const std::array<uint8_t, 2> second = {0xb0, 0xb1};

  binary_log_pipeline_init(&pipeline, pipeline_storage.data(),
                           pipeline_storage.size());
  ASSERT_EQ(uart_capture_service_init(&service, PORT_ID_1, ring_storage.data(),
                                      ring_storage.size(), &pipeline),
            ESP_OK);

  ASSERT_EQ(uart_capture_service_on_rx_bytes(&service, PORT_ID_1, 2000U,
                                             first.data(), first.size()),
            ESP_OK);
  ASSERT_EQ(uart_capture_service_publish_pending(&service, &record), ESP_OK);
  ASSERT_EQ(uart_capture_service_on_rx_bytes(&service, PORT_ID_1, 3000U,
                                             second.data(), second.size()),
            ESP_OK);
  ASSERT_EQ(uart_capture_service_on_rx_bytes(&service, PORT_ID_1, 3200U,
                                             first.data(), 4U),
            ESP_OK);
  ASSERT_EQ(uart_capture_service_publish_pending(&service, &record), ESP_OK);

  const binary_record_header_t header =
      CopyStructAt<binary_record_header_t>(record.bytes, record.length, 0U);
  const uart_data_record_payload_prefix_t prefix =
      CopyStructAt<uart_data_record_payload_prefix_t>(
          record.bytes, record.length, sizeof(header));
  const std::array<uint8_t, 6> expected = {0xb0, 0xb1, 0xa0, 0xa1, 0xa2, 0xa3};

  EXPECT_EQ(header.timestamp_us, 3000U);
  EXPECT_EQ(prefix.data_length, expected.size());
  EXPECT_EQ(0, std::memcmp(record.bytes + sizeof(header) + sizeof(prefix),
                           expected.data(), expected.size()));
}

TEST(UartCaptureServiceTest, EmitsOverflowFaultWhenRingBufferFills) {
  std::array<uint8_t, 512> pipeline_storage = {};
  std::array<uint8_t, 4> ring_storage = {};
  binary_log_pipeline_t pipeline = {};
  uart_capture_service_t service = {};
  const std::array<uint8_t, 5> bytes = {1, 2, 3, 4, 5};
  fault_event_t fault = {};

  binary_log_pipeline_init(&pipeline, pipeline_storage.data(),
                           pipeline_storage.size());
  ASSERT_EQ(uart_capture_service_init(&service, PORT_ID_1, ring_storage.data(),
                                      ring_storage.size(), &pipeline),
            ESP_OK);

  EXPECT_EQ(uart_capture_service_on_rx_bytes(&service, PORT_ID_1, 4000U,
                                             bytes.data(), bytes.size()),
            ESP_ERR_NO_MEM);
  ASSERT_TRUE(uart_capture_service_take_pending_fault(&service, &fault));
  EXPECT_EQ(fault.code, FAULT_CODE_STORAGE_BACKPRESSURE);
  EXPECT_EQ(fault.severity, FAULT_SEVERITY_RECOVERABLE);
}

}  // namespace
