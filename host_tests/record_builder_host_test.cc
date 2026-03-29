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

  config.port_count = 4;
  config.ports[0].enabled = true;
  config.ports[0].baud_rate = 115200;
  config.ports[1].enabled = true;
  config.ports[1].baud_rate = 921600;
  config.ports[2].enabled = false;
  config.ports[3].enabled = true;
  config.ports[3].baud_rate = 460800;

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

}  // namespace
