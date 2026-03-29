#include "gtest/gtest.h"

extern "C" {
#include "fault_manager.h"
}

namespace {

TEST(FaultManagerTest, LatchesDegradedHealthForRecoverableStorageBackpressure) {
  fault_manager_t manager = {};
  fault_event_t event = {};

  fault_manager_init(&manager);

  event.code = FAULT_CODE_STORAGE_BACKPRESSURE;
  event.severity = FAULT_SEVERITY_RECOVERABLE;

  fault_manager_publish(&manager, &event);

  EXPECT_EQ(fault_manager_health(&manager), HEALTH_STATUS_DEGRADED);
  EXPECT_EQ(fault_manager_event_count(&manager), 1U);
  EXPECT_FALSE(fault_manager_has_fatal_fault(&manager));
}

TEST(FaultManagerTest, IgnoresNoneFaultCodeEvents) {
  fault_manager_t manager = {};
  fault_event_t event = {};

  fault_manager_init(&manager);

  event.code = FAULT_CODE_NONE;
  event.severity = FAULT_SEVERITY_RECOVERABLE;

  fault_manager_publish(&manager, &event);

  EXPECT_EQ(fault_manager_health(&manager), HEALTH_STATUS_OK);
  EXPECT_EQ(fault_manager_event_count(&manager), 0U);
  EXPECT_FALSE(fault_manager_has_fatal_fault(&manager));
}

}  // namespace
