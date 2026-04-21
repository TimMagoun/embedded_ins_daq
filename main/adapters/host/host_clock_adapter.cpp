#include "host_clock_adapter.hpp"

namespace daq {

HostClockAdapter::HostClockAdapter(const Timestamp initial_time)
    : now_(initial_time) {}

Timestamp HostClockAdapter::Now() const { return now_; }

void HostClockAdapter::SetNow(const Timestamp now) { now_ = now; }

void HostClockAdapter::AdvanceBy(const Timestamp delta_us) { now_ += delta_us; }

}  // namespace daq
