#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "daq_faults.hpp"
#include "daq_status.hpp"
#include "daq_types.hpp"

TEST(ProjectSkeleton,
     ShouldExposeExpectedCoreTypeShapesGivenSharedPublicTypes) {
  static_assert(std::is_enum_v<daq::State>);
  static_assert(std::is_enum_v<daq::FaultOrigin>);
  static_assert(std::is_enum_v<daq::RecordType>);
  static_assert(std::is_enum_v<daq::PortId>);
  static_assert(
      std::is_same_v<decltype(daq::Timestamp::microseconds), std::uint64_t>);
}

TEST(ProjectSkeleton,
     ShouldStoreReadyStateWithoutFaultGivenFreshStatusSnapshot) {
  daq::StatusSnapshot snapshot{};
  snapshot.state = daq::State::kReady;
  snapshot.active_fault.origin = daq::FaultOrigin::kNone;
  snapshot.active_fault.detail = daq::ConfigFaultDetail::kNone;
  snapshot.session_active = false;

  EXPECT_EQ(snapshot.state, daq::State::kReady);
  EXPECT_EQ(snapshot.active_fault.origin, daq::FaultOrigin::kNone);
}
