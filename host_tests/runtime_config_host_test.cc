#include "gtest/gtest.h"

extern "C" {
#include "board_ports.h"
#include "runtime_config.h"
}

namespace {

runtime_config_t ValidConfig(void) {
  runtime_config_t config = {};

  config.ports[0].enabled = true;
  config.ports[0].timing_mode = PORT_TIMING_TRIGGER;
  config.ports[0].baud_rate = 115200;
  config.ports[0].trigger_period_us = 100;
  config.ports[0].trigger_pulse_width_us = 10;

  return config;
}

TEST(RuntimeConfigTest, RejectsTriggerOutputWithoutPulseWidth) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].trigger_pulse_width_us = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_TRIGGER_PULSE_WIDTH_INVALID);
}

TEST(RuntimeConfigTest, RejectsUnknownTimingMode) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = static_cast<port_timing_mode_t>(99);

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_TIMING_MODE_INVALID);
}

TEST(RuntimeConfigTest, AcceptsSyncConfigurationWithRisingEdge) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = PORT_TIMING_SYNC;
  config.ports[0].trigger_period_us = 0;
  config.ports[0].trigger_pulse_width_us = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, AcceptsSyncConfigurationWithFallingEdge) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = PORT_TIMING_SYNC;
  config.ports[0].sync_edge_mode = SYNC_EDGE_FALLING;
  config.ports[0].trigger_period_us = 0;
  config.ports[0].trigger_pulse_width_us = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, AcceptsSyncConfigurationWithChangeEdge) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = PORT_TIMING_SYNC;
  config.ports[0].sync_edge_mode = SYNC_EDGE_CHANGE;
  config.ports[0].trigger_period_us = 0;
  config.ports[0].trigger_pulse_width_us = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, AcceptsEnabledPortWithNoTimingMode) {
  runtime_config_t config = {};
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  config.ports[0].enabled = true;
  config.ports[0].baud_rate = 115200;
  config.ports[0].timing_mode = PORT_TIMING_NONE;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, AcceptsBuiltInDefaultConfig) {
  runtime_config_t config = runtime_config_default();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, RejectsNullConfig) {
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  EXPECT_EQ(runtime_config_validate(NULL, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NULL_CONFIG);
}

TEST(RuntimeConfigTest, RejectsEmptyPortSet) {
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  runtime_config_t config = {};

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NO_ENABLED_PORTS);
}

TEST(RuntimeConfigTest, RejectsInvalidSyncEdgeMode) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  config.ports[0].timing_mode = PORT_TIMING_SYNC;
  config.ports[0].sync_edge_mode = static_cast<sync_edge_mode_t>(99);
  config.ports[0].trigger_period_us = 0;
  config.ports[0].trigger_pulse_width_us = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_SYNC_EDGE_MODE_INVALID);
}

TEST(RuntimeConfigTest, RejectsDisabledBaudRate) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].baud_rate = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_BAUD_RATE_INVALID);
}

TEST(RuntimeConfigTest, ReportsValidMessage) {
  EXPECT_STREQ(runtime_config_error_message(RUNTIME_CONFIG_ERROR_NONE),
               "runtime config is valid");
}

TEST(RuntimeConfigTest, ReportsNullConfigMessage) {
  EXPECT_STREQ(runtime_config_error_message(RUNTIME_CONFIG_ERROR_NULL_CONFIG),
               "runtime config pointer is null");
}

TEST(RuntimeConfigTest, ReportsNoEnabledPortsMessage) {
  EXPECT_STREQ(
      runtime_config_error_message(RUNTIME_CONFIG_ERROR_NO_ENABLED_PORTS),
      "runtime config must enable at least one port");
}

TEST(RuntimeConfigTest, ReportsBaudRateMessage) {
  EXPECT_STREQ(
      runtime_config_error_message(RUNTIME_CONFIG_ERROR_BAUD_RATE_INVALID),
      "runtime config baud rate must be positive");
}

TEST(RuntimeConfigTest, ReportsTimingModeMessage) {
  EXPECT_STREQ(
      runtime_config_error_message(RUNTIME_CONFIG_ERROR_TIMING_MODE_INVALID),
      "runtime config timing mode is invalid");
}

TEST(RuntimeConfigTest, ReportsSyncEdgeModeMessage) {
  EXPECT_STREQ(
      runtime_config_error_message(RUNTIME_CONFIG_ERROR_SYNC_EDGE_MODE_INVALID),
      "runtime config sync edge mode is invalid");
}

TEST(RuntimeConfigTest, ReportsTriggerPulseWidthMessage) {
  EXPECT_STREQ(runtime_config_error_message(
                   RUNTIME_CONFIG_ERROR_TRIGGER_PULSE_WIDTH_INVALID),
               "runtime trigger pulse width must be non-zero and less than its "
               "period");
}

TEST(RuntimeConfigTest, ReportsUnknownMessage) {
  EXPECT_STREQ(
      runtime_config_error_message(static_cast<runtime_config_error_t>(99)),
      "unknown runtime config validation result");
}

}  // namespace
