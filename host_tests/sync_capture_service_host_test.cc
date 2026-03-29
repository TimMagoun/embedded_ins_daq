#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "binary_log_pipeline.h"
#include "fault_manager.h"
#include "sync_capture_service.h"
}

namespace {

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

std::vector<uint8_t> DrainPipeline(binary_log_pipeline_t* pipeline) {
  std::array<uint8_t, 128> flush_buffer = {};
  std::vector<uint8_t> drained;
  size_t bytes_written = 0U;

  while (binary_log_pipeline_pending_bytes(pipeline) > 0U) {
    EXPECT_EQ(binary_log_pipeline_flush(pipeline, flush_buffer.data(),
                                        flush_buffer.size(), &bytes_written),
              ESP_OK);
    drained.insert(drained.end(), flush_buffer.begin(),
                   flush_buffer.begin() + bytes_written);
  }

  return drained;
}

TEST(SyncCaptureServiceTest, PreservesObservedEdgePolarity) {
  std::array<uint8_t, 512> pipeline_storage = {};
  std::array<sync_edge_event_t, 4> queue_storage = {};
  binary_log_pipeline_t pipeline = {};
  fault_manager_t faults = {};
  sync_capture_service_t service = {};

  binary_log_pipeline_init(&pipeline, pipeline_storage.data(),
                           pipeline_storage.size());
  fault_manager_init(&faults);
  ASSERT_EQ(sync_capture_service_init(&service, queue_storage.data(),
                                      queue_storage.size()),
            ESP_OK);

  ASSERT_EQ(sync_capture_service_publish_isr(&service, PORT_ID_3, 1234U, true),
            ESP_OK);
  ASSERT_EQ(sync_capture_service_drain(&service, &pipeline, &faults), ESP_OK);

  const std::vector<uint8_t> drained = DrainPipeline(&pipeline);
  const binary_record_header_t header =
      CopyStructAt<binary_record_header_t>(drained, 0U);
  const sync_edge_record_payload_t payload =
      CopyStructAt<sync_edge_record_payload_t>(drained, sizeof(header));

  EXPECT_EQ(header.record_type, RECORD_TYPE_SYNC_EDGE);
  EXPECT_EQ(header.timestamp_us, 1234U);
  EXPECT_EQ(header.source_id, PORT_ID_3);
  EXPECT_EQ(payload.edge_polarity, 1U);
}

TEST(SyncCaptureServiceTest, EmitsOverflowFaultWhenEdgeQueueFills) {
  std::array<uint8_t, 512> pipeline_storage = {};
  std::array<sync_edge_event_t, 1> queue_storage = {};
  binary_log_pipeline_t pipeline = {};
  fault_manager_t faults = {};
  sync_capture_service_t service = {};

  binary_log_pipeline_init(&pipeline, pipeline_storage.data(),
                           pipeline_storage.size());
  fault_manager_init(&faults);
  ASSERT_EQ(sync_capture_service_init(&service, queue_storage.data(),
                                      queue_storage.size()),
            ESP_OK);

  ASSERT_EQ(sync_capture_service_publish_isr(&service, PORT_ID_1, 100U, true),
            ESP_OK);
  EXPECT_EQ(sync_capture_service_publish_isr(&service, PORT_ID_1, 200U, false),
            ESP_ERR_NO_MEM);
  ASSERT_EQ(sync_capture_service_drain(&service, &pipeline, &faults), ESP_OK);

  EXPECT_EQ(fault_manager_event_count(&faults), 1U);
  EXPECT_EQ(fault_manager_health(&faults), HEALTH_STATUS_DEGRADED);
}

}  // namespace
