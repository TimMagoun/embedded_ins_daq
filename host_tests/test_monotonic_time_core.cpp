#include <gtest/gtest.h>

#include <array>

#include "daq_types.hpp"
#include "host_clock_adapter.hpp"
#include "monotonic_time_core.hpp"

namespace {

using daq::PortId;
using daq::SignalEdge;
using daq::StorageWriteBlock;
using daq::Timestamp;
using daq::TimingEventKind;
using daq::TimingEventRecord;
using daq::UartChunkRecord;

constexpr Timestamp kBaseTime = 1'000U;

TEST(MonotonicTimeCore,
     ShouldReportElapsedMicrosGivenLargeTimestampDeltaWithoutOverflow) {
  constexpr Timestamp start = 17U;
  constexpr Timestamp end = (static_cast<Timestamp>(1) << 63) + 99U;

  EXPECT_EQ(daq::ElapsedMicros(start, end), end - start);
}

TEST(MonotonicTimeCore,
     ShouldExpireIdleGapGivenElapsedMicrosAtOrBeyondThreshold) {
  constexpr Timestamp last_byte = 10'000U;

  EXPECT_FALSE(daq::HasIdleGapExpired(last_byte, last_byte + 249U, 250U));
  EXPECT_TRUE(daq::HasIdleGapExpired(last_byte, last_byte + 250U, 250U));
}

TEST(MonotonicTimeCore,
     ShouldPreserveFirstByteTimestampGivenSubsequentBytesInSameChunk) {
  constexpr Timestamp first_byte = 123'456U;
  constexpr Timestamp later_byte = first_byte + 9U;

  UartChunkRecord record{PortId::kSensor2, first_byte, later_byte, nullptr, 0U};

  EXPECT_EQ(record.first_byte_timestamp, first_byte);
  EXPECT_EQ(record.last_byte_timestamp, later_byte);
}

TEST(MonotonicTimeCore, ShouldReadDeterministicTimeGivenHostAdapterNow) {
  daq::HostClockAdapter adapter(kBaseTime);
  EXPECT_EQ(adapter.Now(), kBaseTime);

  adapter.AdvanceBy(25U);

  const Timestamp now = adapter.Now();
  ASSERT_EQ(now, kBaseTime + 25U);
  EXPECT_TRUE(daq::HasIdleGapExpired(kBaseTime, now, 25U));
}

TEST(MonotonicTimeCore,
     ShouldRetainPayloadPointerAndLengthGivenStorageWriteBlockView) {
  static constexpr std::array<std::uint8_t, 3> bytes{{0x01U, 0x02U, 0x03U}};
  StorageWriteBlock block{bytes.data(),
                          static_cast<std::uint16_t>(bytes.size()), false};

  ASSERT_NE(block.data, nullptr);
  EXPECT_EQ(block.size, bytes.size());
  EXPECT_EQ(block.data[1], 0x02U);
}

}  // namespace
