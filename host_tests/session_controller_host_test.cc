#include "gtest/gtest.h"

extern "C" {
#include "fault_manager.h"
#include "runtime_config.h"
#include "session_controller.h"
}

namespace {

runtime_config_t ValidConfig(void) {
  runtime_config_t config = {};

  config.ports[0].enabled = true;
  config.ports[0].baud_rate = 460800;
  config.ports[0].timing_mode = PORT_TIMING_NONE;
  config.ports[0].sync_edge_mode = SYNC_EDGE_RISING;

  return config;
}

TEST(SessionControllerTest, BlocksStartWhenConfigInvalid) {
  session_controller_t controller = {};
  runtime_config_t invalid = {};

  session_controller_init(&controller);

  EXPECT_FALSE(session_controller_request_start(&controller, &invalid));
  EXPECT_EQ(session_controller_state(&controller), SESSION_CONFIG_INVALID);
  EXPECT_EQ(session_controller_last_config_error(&controller),
            RUNTIME_CONFIG_ERROR_NO_ENABLED_PORTS);
}

TEST(SessionControllerTest, TransitionsToFaultedOnFatalFault) {
  session_controller_t controller = {};
  runtime_config_t config = ValidConfig();
  fault_event_t fatal_fault = {};

  session_controller_init(&controller);
  ASSERT_TRUE(session_controller_request_start(&controller, &config));
  ASSERT_EQ(session_controller_mark_recording(&controller), ESP_OK);

  fatal_fault.code = FAULT_CODE_STORAGE_IO;
  fatal_fault.severity = FAULT_SEVERITY_FATAL;

  session_controller_publish_fault(&controller, &fatal_fault);

  EXPECT_EQ(session_controller_state(&controller), SESSION_FAULTED);
}

TEST(SessionControllerTest, EntersReadyOnlyAfterStorageMountAndConfigLoad) {
  session_controller_t controller = {};
  runtime_config_t config = ValidConfig();

  session_controller_init(&controller);

  EXPECT_EQ(session_controller_state(&controller), SESSION_BOOT);
  EXPECT_EQ(session_controller_mark_config_loaded(&controller, &config),
            ESP_OK);
  EXPECT_EQ(session_controller_state(&controller), SESSION_BOOT);
  EXPECT_EQ(session_controller_mark_storage_ready(&controller), ESP_OK);
  EXPECT_EQ(session_controller_state(&controller), SESSION_READY);
}

TEST(SessionControllerTest, RejectsConfigLoadAfterRecordingStarts) {
  session_controller_t controller = {};
  runtime_config_t config = ValidConfig();

  session_controller_init(&controller);

  ASSERT_TRUE(session_controller_request_start(&controller, &config));
  ASSERT_EQ(session_controller_mark_recording(&controller), ESP_OK);
  EXPECT_EQ(session_controller_mark_config_loaded(&controller, &config),
            ESP_ERR_INVALID_STATE);
  EXPECT_EQ(session_controller_mark_storage_ready(&controller),
            ESP_ERR_INVALID_STATE);
  EXPECT_EQ(session_controller_state(&controller), SESSION_RECORDING);
}

TEST(SessionControllerTest, RejectsRepeatStartAfterRecordingStarts) {
  session_controller_t controller = {};
  runtime_config_t config = ValidConfig();

  session_controller_init(&controller);

  ASSERT_TRUE(session_controller_request_start(&controller, &config));
  ASSERT_EQ(session_controller_mark_recording(&controller), ESP_OK);
  EXPECT_FALSE(session_controller_request_start(&controller, &config));
  EXPECT_EQ(session_controller_state(&controller), SESSION_RECORDING);
}

}  // namespace
