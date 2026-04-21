#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "daq_types.hpp"

TEST(ProjectSkeleton,
     ShouldExposeExpectedCoreTypeShapesGivenSharedPublicTypes) {
  static_assert(std::is_enum_v<daq::State>);
  static_assert(std::is_enum_v<daq::ComponentName>);
  static_assert(std::is_enum_v<daq::RecordType>);
  static_assert(std::is_enum_v<daq::PortId>);
  static_assert(std::is_same_v<daq::Timestamp, std::uint64_t>);
}
