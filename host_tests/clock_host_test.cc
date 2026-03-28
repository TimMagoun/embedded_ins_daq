#include <cstddef>
#include <cstdint>

#include "gtest/gtest.h"

extern "C" {
#include "clock_probe.h"
}

namespace {

struct FakeClock {
  const uint64_t* values;
  size_t count;
  size_t index;
};

uint64_t FakeClockRead(void* ctx) {
  auto* clock = static_cast<FakeClock*>(ctx);
  uint64_t value;

  if (clock->count == 0U) {
    return 0U;
  }

  value = clock->values[clock->index];
  if (clock->index + 1U < clock->count) {
    clock->index += 1U;
  }
  return value;
}

TEST(ClockProbeTest, RejectsNullReader) {
  uint64_t last_sample = 99U;

  EXPECT_FALSE(clock_probe_monotonic(nullptr, nullptr, 4U, &last_sample));
  EXPECT_EQ(last_sample, 0U);
}

TEST(ClockProbeTest, RejectsZeroSamples) {
  static const uint64_t monotonic_values[] = {1U, 2U};
  FakeClock fake_clock = {
      .values = monotonic_values,
      .count = std::size(monotonic_values),
      .index = 0U,
  };
  uint64_t last_sample = 99U;

  EXPECT_FALSE(
      clock_probe_monotonic(FakeClockRead, &fake_clock, 0U, &last_sample));
  EXPECT_EQ(last_sample, 0U);
}

TEST(ClockProbeTest, AcceptsMonotonicSequence) {
  static const uint64_t monotonic_values[] = {10U, 11U, 11U, 25U, 80U, 81U};
  FakeClock fake_clock = {
      .values = monotonic_values,
      .count = std::size(monotonic_values),
      .index = 0U,
  };
  uint64_t last_sample = 0U;

  EXPECT_TRUE(clock_probe_monotonic(FakeClockRead, &fake_clock,
                                    fake_clock.count, &last_sample));
  EXPECT_EQ(last_sample, 81U);
}

TEST(ClockProbeTest, ReportsFirstDecreasingSample) {
  static const uint64_t non_monotonic_values[] = {10U, 12U, 9U, 20U};
  FakeClock fake_clock = {
      .values = non_monotonic_values,
      .count = std::size(non_monotonic_values),
      .index = 0U,
  };
  uint64_t last_sample = 0U;

  EXPECT_FALSE(clock_probe_monotonic(FakeClockRead, &fake_clock,
                                     fake_clock.count, &last_sample));
  EXPECT_EQ(last_sample, 9U);
}

TEST(ClockProbeTest, ExtendsWithoutWrap) {
  EXPECT_EQ(clock_probe_extend_low_word(0x00000000fffffffeULL, 0xffffffffU),
            0x00000000ffffffffULL);
}

TEST(ClockProbeTest, ExtendsAcrossWrap) {
  EXPECT_EQ(clock_probe_extend_low_word(0x00000000ffffffffULL, 0x00000002U),
            0x0000000100000002ULL);
}

TEST(ClockProbeTest, ConvertsTicksToMicroseconds) {
  EXPECT_EQ(clock_probe_ticks_to_us(2000U, 2000000U), 1000U);
}

TEST(ClockProbeTest, TruncatesFractionalMicroseconds) {
  EXPECT_EQ(clock_probe_ticks_to_us(3U, 2U), 1500000U);
}

TEST(ClockProbeTest, ReturnsZeroWhenResolutionIsZero) {
  EXPECT_EQ(clock_probe_ticks_to_us(123U, 0U), 0U);
}

}  // namespace
