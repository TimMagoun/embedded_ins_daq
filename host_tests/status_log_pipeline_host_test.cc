#include <array>
#include <string>

#include "gtest/gtest.h"

extern "C" {
#include "status_log_pipeline.h"
}

namespace {

TEST(StatusLogPipelineTest, AppendsNewlinesAndPreservesTerminatorAfterFlush) {
  std::array<char, 32> staging = {};
  std::array<char, 5> flush_buffer = {};
  status_log_pipeline_t pipeline = {};
  size_t bytes_written = 0U;
  std::string drained;

  status_log_pipeline_init(&pipeline, staging.data(), staging.size());

  ASSERT_EQ(status_log_pipeline_append(&pipeline, "alpha"), ESP_OK);
  ASSERT_EQ(status_log_pipeline_append(&pipeline, "beta"), ESP_OK);
  EXPECT_STREQ(staging.data(), "alpha\nbeta\n");

  while (status_log_pipeline_pending_bytes(&pipeline) > 0U) {
    ASSERT_EQ(status_log_pipeline_flush(&pipeline, flush_buffer.data(),
                                        flush_buffer.size(), &bytes_written),
              ESP_OK);
    drained.append(flush_buffer.data(), bytes_written);
    EXPECT_EQ(staging[pipeline.length], '\0');
  }

  EXPECT_EQ(drained, "alpha\nbeta\n");
  EXPECT_EQ(status_log_pipeline_pending_bytes(&pipeline), 0U);
  EXPECT_EQ(staging[0], '\0');
}

TEST(StatusLogPipelineTest,
     RejectsAppendThatWouldExceedCapacityIncludingNewline) {
  std::array<char, 6> staging = {};
  status_log_pipeline_t pipeline = {};

  status_log_pipeline_init(&pipeline, staging.data(), staging.size());

  ASSERT_EQ(status_log_pipeline_append(&pipeline, "alpha"), ESP_OK);
  EXPECT_EQ(status_log_pipeline_append(&pipeline, "b"), ESP_ERR_NO_MEM);
  EXPECT_STREQ(staging.data(), "alpha\n");
}

}  // namespace
