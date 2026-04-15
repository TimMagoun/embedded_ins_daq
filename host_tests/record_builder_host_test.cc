#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"

extern "C" {
#include "record_builder.h"
#include "runtime_config.h"
}

namespace {

runtime_config_t SampleConfig(void) {
  runtime_config_t config = {};

  config.ports[0].enabled = true;
  config.ports[0].baud_rate = 115200;
  config.ports[0].timing_mode = PORT_TIMING_NONE;
  config.ports[0].sync_edge_mode = SYNC_EDGE_RISING;
  config.ports[1].enabled = true;
  config.ports[1].baud_rate = 921600;
  config.ports[1].timing_mode = PORT_TIMING_SYNC;
  config.ports[1].sync_edge_mode = SYNC_EDGE_FALLING;
  config.ports[2].enabled = false;
  config.ports[3].enabled = true;
  config.ports[3].baud_rate = 460800;
  config.ports[3].timing_mode = PORT_TIMING_TRIGGER;
  config.ports[3].sync_edge_mode = SYNC_EDGE_CHANGE;
  config.ports[3].trigger_period_us = 1000;
  config.ports[3].trigger_pulse_width_us = 50;

  return config;
}

template <typename T>
T CopyStructAt(const record_buffer_t& record, size_t offset) {
  T value = {};
  EXPECT_LE(offset + sizeof(T), record.length);
  if (offset + sizeof(T) > record.length) {
    return value;
  }
  std::memcpy(&value, record.bytes + offset, sizeof(T));
  return value;
}

TEST(RecordBuilderTest, EncodesSessionStartWithConfigHashAndPortSummary) {
  record_buffer_t record = {};
  session_info_t session = {};
  runtime_config_t config = SampleConfig();

  session.session_id = 7;
  session.start_timestamp_us = 123456ULL;

  ASSERT_EQ(record_builder_build_session_start(&session, &config, &record),
            ESP_OK);

  const binary_record_header_t header =
      CopyStructAt<binary_record_header_t>(record, 0);
  const session_start_record_payload_t payload =
      CopyStructAt<session_start_record_payload_t>(record, sizeof(header));

  EXPECT_EQ(header.record_type, RECORD_TYPE_SESSION_START);
  EXPECT_EQ(header.record_version, RECORD_FORMAT_VERSION);
  EXPECT_EQ(header.timestamp_us, session.start_timestamp_us);
  EXPECT_EQ(payload.session_id, session.session_id);
  EXPECT_EQ(payload.config_hash, record_builder_config_hash(&config));
  EXPECT_EQ(payload.enabled_port_count, 3U);
  EXPECT_EQ(payload.enabled_port_mask, 0x0bU);
}

TEST(RecordBuilderTest, EncodesUartDataWithFirstByteTimestamp) {
  record_buffer_t record = {};
  const std::array<uint8_t, 4> bytes = {0xde, 0xad, 0xbe, 0xef};
  const uint64_t first_byte_timestamp_us = 987654321ULL;

  ASSERT_EQ(record_builder_build_uart_data(2, first_byte_timestamp_us,
                                           bytes.data(), bytes.size(), &record),
            ESP_OK);

  const binary_record_header_t header =
      CopyStructAt<binary_record_header_t>(record, 0);
  const uart_data_record_payload_prefix_t payload =
      CopyStructAt<uart_data_record_payload_prefix_t>(record, sizeof(header));
  std::array<uint8_t, 4> encoded = {};
  std::memcpy(encoded.data(), record.bytes + sizeof(header) + sizeof(payload),
              encoded.size());

  EXPECT_EQ(header.record_type, RECORD_TYPE_UART_DATA);
  EXPECT_EQ(header.timestamp_us, first_byte_timestamp_us);
  EXPECT_EQ(header.source_id, 2U);
  EXPECT_EQ(payload.data_length, bytes.size());
  EXPECT_EQ(encoded, bytes);
}

TEST(RecordBuilderTest, ReturnsZeroForNullConfigHash) {
  EXPECT_EQ(record_builder_config_hash(NULL), 0U);
}

TEST(RecordBuilderTest, RejectsNullSessionStartSession) {
  record_buffer_t record = {};
  runtime_config_t config = SampleConfig();

  EXPECT_EQ(record_builder_build_session_start(NULL, &config, &record),
            ESP_ERR_INVALID_ARG);
}

TEST(RecordBuilderTest, RejectsNullSessionStartConfig) {
  record_buffer_t record = {};
  session_info_t session = {};

  EXPECT_EQ(record_builder_build_session_start(&session, NULL, &record),
            ESP_ERR_INVALID_ARG);
}

TEST(RecordBuilderTest, RejectsNullSessionStartOutput) {
  runtime_config_t config = SampleConfig();
  session_info_t session = {};

  EXPECT_EQ(record_builder_build_session_start(&session, &config, NULL),
            ESP_ERR_INVALID_ARG);
}

TEST(RecordBuilderTest, RejectsNullFaultEventInput) {
  record_buffer_t record = {};

  EXPECT_EQ(
      record_builder_build_fault_event(1U, 2U, NULL, HEALTH_STATUS_OK, &record),
      ESP_ERR_INVALID_ARG);
}

TEST(RecordBuilderTest, RejectsNullFaultEventOutput) {
  fault_event_t fault_event = {};

  EXPECT_EQ(record_builder_build_fault_event(1U, 2U, &fault_event,
                                             HEALTH_STATUS_OK, NULL),
            ESP_ERR_INVALID_ARG);
}

TEST(RecordBuilderTest, RejectsNullUartDataBytes) {
  record_buffer_t record = {};

  EXPECT_EQ(record_builder_build_uart_data(1U, 2U, NULL, 4U, &record),
            ESP_ERR_INVALID_ARG);
}

TEST(RecordBuilderTest, RejectsZeroLengthUartData) {
  record_buffer_t record = {};
  const std::array<uint8_t, 4> bytes = {0xde, 0xad, 0xbe, 0xef};

  EXPECT_EQ(record_builder_build_uart_data(1U, 2U, bytes.data(), 0U, &record),
            ESP_ERR_INVALID_ARG);
}

TEST(RecordBuilderTest, RejectsOversizedUartData) {
  record_buffer_t record = {};
  const std::array<uint8_t, RECORD_BUFFER_CAPACITY_BYTES> oversized_bytes = {};

  EXPECT_EQ(record_builder_build_uart_data(1U, 2U, oversized_bytes.data(),
                                           oversized_bytes.size(), &record),
            ESP_ERR_NO_MEM);
}

TEST(RecordBuilderTest, RejectsNullSyncEdgeOutput) {
  EXPECT_EQ(record_builder_build_sync_edge(1U, 2U, true, NULL),
            ESP_ERR_INVALID_ARG);
}

TEST(RecordBuilderTest, EncodesFaultEvent) {
  record_buffer_t record = {};
  fault_event_t fault_event = {};

  fault_event.code = FAULT_CODE_CAPTURE_OVERFLOW;
  fault_event.severity = FAULT_SEVERITY_FATAL;

  ASSERT_EQ(record_builder_build_fault_event(3U, 4U, &fault_event,
                                             HEALTH_STATUS_FAULTED, &record),
            ESP_OK);
}

TEST(RecordBuilderTest, EncodesSyncEdge) {
  record_buffer_t record = {};

  ASSERT_EQ(record_builder_build_sync_edge(5U, 6U, false, &record), ESP_OK);
}

}  // namespace
