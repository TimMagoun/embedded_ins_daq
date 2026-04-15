#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "log_pipeline_base.h"
}

namespace {

TEST(LogPipelineBaseTest, PreservesAppendOrderAcrossFlushBoundaries) {
  std::array<uint8_t, 8> staging = {};
  std::array<uint8_t, 3> flush_buffer = {};
  log_pipeline_base_t pipeline = {};
  std::vector<uint8_t> drained;
  size_t bytes_written = 0U;

  const std::array<uint8_t, 3> first = {1U, 2U, 3U};
  const std::array<uint8_t, 4> second = {4U, 5U, 6U, 7U};

  log_pipeline_base_init(&pipeline, staging.data(), staging.size());

  ASSERT_EQ(log_pipeline_base_append(&pipeline, first.data(), first.size()),
            ESP_OK);
  ASSERT_EQ(log_pipeline_base_append(&pipeline, second.data(), second.size()),
            ESP_OK);

  while (log_pipeline_base_pending_bytes(&pipeline) > 0U) {
    ASSERT_EQ(log_pipeline_base_flush(&pipeline, flush_buffer.data(),
                                      flush_buffer.size(), &bytes_written),
              ESP_OK);
    drained.insert(drained.end(), flush_buffer.begin(),
                   flush_buffer.begin() + bytes_written);
  }

  const std::vector<uint8_t> expected = {1U, 2U, 3U, 4U, 5U, 6U, 7U};
  EXPECT_EQ(drained, expected);
}

TEST(LogPipelineBaseTest, RejectsAppendWhenStagingIsFull) {
  std::array<uint8_t, 4> staging = {};
  log_pipeline_base_t pipeline = {};
  const std::array<uint8_t, 4> bytes = {9U, 8U, 7U, 6U};
  const uint8_t extra = 6U;

  log_pipeline_base_init(&pipeline, staging.data(), staging.size());

  ASSERT_EQ(log_pipeline_base_append(&pipeline, bytes.data(), bytes.size()),
            ESP_OK);
  EXPECT_EQ(log_pipeline_base_append(&pipeline, &extra, sizeof(extra)),
            ESP_ERR_NO_MEM);
}

}  // namespace
