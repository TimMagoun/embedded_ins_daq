#include <cassert>
#include <cstdint>
#include <type_traits>

#include "c_linkage.h"
#include "daq_faults.hpp"
#include "daq_status.hpp"
#include "daq_types.hpp"

BEGIN_EXTERN_C
void app_main(void);
END_EXTERN_C

int main() {
  static_assert(std::is_enum_v<daq::State>);
  static_assert(std::is_enum_v<daq::FaultOrigin>);
  static_assert(std::is_enum_v<daq::RecordType>);
  static_assert(std::is_enum_v<daq::PortId>);
  static_assert(
      std::is_same_v<decltype(daq::Timestamp::microseconds), std::uint64_t>);

  const daq::StatusSnapshot snapshot{
      .state = daq::State::kIdle,
      .active_fault = {.origin = daq::FaultOrigin::kNone, .detail = 0},
      .session_active = false,
  };

  assert(snapshot.state == daq::State::kIdle);
  assert(snapshot.active_fault.origin == daq::FaultOrigin::kNone);
  assert(app_main != nullptr);

  return 0;
}
