#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"

extern "C" {
#include "binary_log_pipeline.h"
#include "fault_manager.h"
#include "runtime_config.h"
#include "sync_capture_service.h"
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

TEST(TwoPortCaptureTest, KeepsPortCountersAndChunksIsolated) {
  std::array<uint8_t, 512> pipeline_storage = {};
  std::array<uint8_t, 32> port1_ring = {};
  std::array<uint8_t, 32> port2_ring = {};
  binary_log_pipeline_t pipeline = {};
  uart_capture_service_t port1 = {};
  uart_capture_service_t port2 = {};
  uart_capture_stats_t port1_stats = {};
  uart_capture_stats_t port2_stats = {};
  record_buffer_t port1_record = {};
  record_buffer_t port2_record = {};
  const std::array<uint8_t, 3> port1_bytes = {0x10, 0x11, 0x12};
  const std::array<uint8_t, 4> port2_bytes = {0x20, 0x21, 0x22, 0x23};

  binary_log_pipeline_init(&pipeline, pipeline_storage.data(),
                           pipeline_storage.size());
  ASSERT_EQ(uart_capture_service_init(&port1, PORT_ID_1, port1_ring.data(),
                                      port1_ring.size(), &pipeline),
            ESP_OK);
  ASSERT_EQ(uart_capture_service_init(&port2, PORT_ID_2, port2_ring.data(),
                                      port2_ring.size(), &pipeline),
            ESP_OK);

  ASSERT_EQ(
      uart_capture_service_on_rx_bytes(&port1, PORT_ID_1, 1000U,
                                       port1_bytes.data(), port1_bytes.size()),
      ESP_OK);
  ASSERT_EQ(
      uart_capture_service_on_rx_bytes(&port2, PORT_ID_2, 2000U,
                                       port2_bytes.data(), port2_bytes.size()),
      ESP_OK);
  ASSERT_EQ(uart_capture_service_publish_pending(&port1, &port1_record),
            ESP_OK);
  ASSERT_EQ(uart_capture_service_get_stats(&port1, PORT_ID_1, &port1_stats),
            ESP_OK);
  ASSERT_EQ(uart_capture_service_get_stats(&port2, PORT_ID_2, &port2_stats),
            ESP_OK);

  EXPECT_EQ(port1_stats.total_bytes_captured, port1_bytes.size());
  EXPECT_EQ(port1_stats.pending_bytes, 0U);
  EXPECT_EQ(port1_stats.published_chunks, 1U);
  EXPECT_EQ(port2_stats.total_bytes_captured, port2_bytes.size());
  EXPECT_EQ(port2_stats.pending_bytes, port2_bytes.size());
  EXPECT_EQ(port2_stats.published_chunks, 0U);

  ASSERT_EQ(uart_capture_service_publish_pending(&port2, &port2_record),
            ESP_OK);

  const binary_record_header_t port1_header =
      CopyStructAt<binary_record_header_t>(port1_record.bytes,
                                           port1_record.length, 0U);
  const binary_record_header_t port2_header =
      CopyStructAt<binary_record_header_t>(port2_record.bytes,
                                           port2_record.length, 0U);
  const uart_data_record_payload_prefix_t port1_prefix =
      CopyStructAt<uart_data_record_payload_prefix_t>(
          port1_record.bytes, port1_record.length, sizeof(port1_header));
  const uart_data_record_payload_prefix_t port2_prefix =
      CopyStructAt<uart_data_record_payload_prefix_t>(
          port2_record.bytes, port2_record.length, sizeof(port2_header));

  EXPECT_EQ(port1_header.source_id, PORT_ID_1);
  EXPECT_EQ(port2_header.source_id, PORT_ID_2);
  EXPECT_EQ(port1_prefix.data_length, port1_bytes.size());
  EXPECT_EQ(port2_prefix.data_length, port2_bytes.size());
  EXPECT_EQ(0, std::memcmp(port1_record.bytes + sizeof(port1_header) +
                               sizeof(port1_prefix),
                           port1_bytes.data(), port1_bytes.size()));
  EXPECT_EQ(0, std::memcmp(port2_record.bytes + sizeof(port2_header) +
                               sizeof(port2_prefix),
                           port2_bytes.data(), port2_bytes.size()));
}

TEST(TwoPortCaptureTest, SupportsIndependentBaudRatesPerPort) {
  runtime_config_t config = {};
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  config.port_count = 2;
  config.ports[0].enabled = true;
  config.ports[0].baud_rate = 9600;
  config.ports[0].timing_mode = PORT_TIMING_DISABLED;
  config.ports[1].enabled = true;
  config.ports[1].baud_rate = 921600;
  config.ports[1].timing_mode = PORT_TIMING_DISABLED;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(TwoPortCaptureTest, SupportsFourPortCaptureAlongsideSyncEvents) {
  std::array<uint8_t, 2048> pipeline_storage = {};
  std::array<std::array<uint8_t, 32>, 4> port_rings = {};
  std::array<std::array<uint8_t, 2>, 4> port_bytes = {{
      {0x10, 0x11},
      {0x20, 0x21},
      {0x30, 0x31},
      {0x40, 0x41},
  }};
  std::array<uart_capture_service_t, 4> capture = {};
  std::array<record_buffer_t, 4> records = {};
  std::array<sync_edge_event_t, 4> sync_queue = {};
  binary_log_pipeline_t pipeline = {};
  sync_capture_service_t sync_service = {};
  fault_manager_t faults = {};

  binary_log_pipeline_init(&pipeline, pipeline_storage.data(),
                           pipeline_storage.size());
  fault_manager_init(&faults);
  ASSERT_EQ(sync_capture_service_init(&sync_service, sync_queue.data(),
                                      sync_queue.size()),
            ESP_OK);

  for (size_t i = 0; i < capture.size(); ++i) {
    ASSERT_EQ(uart_capture_service_init(&capture[i], (port_id_t)(i + 1U),
                                        port_rings[i].data(),
                                        port_rings[i].size(), &pipeline),
              ESP_OK);
    ASSERT_EQ(uart_capture_service_on_rx_bytes(
                  &capture[i], (port_id_t)(i + 1U), 1000U + (i * 100U),
                  port_bytes[i].data(), port_bytes[i].size()),
              ESP_OK);
    ASSERT_EQ(uart_capture_service_publish_pending(&capture[i], &records[i]),
              ESP_OK);
  }

  ASSERT_EQ(
      sync_capture_service_publish_isr(&sync_service, PORT_ID_4, 1500U, true),
      ESP_OK);
  ASSERT_EQ(sync_capture_service_drain(&sync_service, &pipeline, &faults),
            ESP_OK);

  EXPECT_EQ(binary_log_pipeline_pending_bytes(&pipeline),
            records[0].length + records[1].length + records[2].length +
                records[3].length + sizeof(binary_record_header_t) +
                sizeof(sync_edge_record_payload_t));
  EXPECT_EQ(fault_manager_event_count(&faults), 0U);
}

}  // namespace
