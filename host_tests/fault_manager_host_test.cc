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

TEST(FaultManagerTest, IgnoresNullManagerOnInit) { fault_manager_init(NULL); }

TEST(FaultManagerTest, IgnoresNullInputsOnPublish) {
  fault_manager_t manager = {};
  fault_event_t event = {};

  fault_manager_init(&manager);
  fault_manager_publish(NULL, &event);
  fault_manager_publish(&manager, NULL);

  EXPECT_EQ(fault_manager_health(&manager), HEALTH_STATUS_OK);
  EXPECT_EQ(fault_manager_event_count(&manager), 0U);
  EXPECT_FALSE(fault_manager_has_fatal_fault(&manager));
}

TEST(FaultManagerTest, ReportsNullManagerAsFaulted) {
  EXPECT_EQ(fault_manager_health(NULL), HEALTH_STATUS_FAULTED);
  EXPECT_EQ(fault_manager_event_count(NULL), 0U);
  EXPECT_TRUE(fault_manager_has_fatal_fault(NULL));
}

TEST(FaultManagerTest, LatchesFatalFaults) {
  fault_manager_t manager = {};
  fault_event_t fatal_event = {};

  fault_manager_init(&manager);
  fatal_event.code = FAULT_CODE_CAPTURE_OVERFLOW;
  fatal_event.severity = FAULT_SEVERITY_FATAL;

  fault_manager_publish(&manager, &fatal_event);

  EXPECT_EQ(fault_manager_health(&manager), HEALTH_STATUS_FAULTED);
  EXPECT_EQ(fault_manager_event_count(&manager), 1U);
  EXPECT_TRUE(fault_manager_has_fatal_fault(&manager));
}

}  // namespace
