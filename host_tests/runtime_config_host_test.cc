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

TEST(RuntimeConfigTest, RejectsDisabledBaudRate) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].baud_rate = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_BAUD_RATE_INVALID);
}

TEST(RuntimeConfigTest, RejectsUnknownTimingMode) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = static_cast<port_timing_mode_t>(99);

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_TIMING_MODE_INVALID);
}

TEST(RuntimeConfigTest, AcceptsSyncConfigurationWithoutExplicitEdgeMode) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = PORT_TIMING_SYNC;
  config.ports[0].trigger_period_us = 0;
  config.ports[0].trigger_pulse_width_us = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, AcceptsExplicitSyncEdgeModes) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = PORT_TIMING_SYNC;
  config.ports[0].sync_edge_mode = SYNC_EDGE_FALLING;
  config.ports[0].trigger_period_us = 0;
  config.ports[0].trigger_pulse_width_us = 0;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
  config.ports[0].sync_edge_mode = SYNC_EDGE_CHANGE;
  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, AcceptsReferenceGnssAndImuPortProfileSet) {
  runtime_config_t config = {};
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  config.ports[0].enabled = true;
  config.ports[0].baud_rate = 9600;
  config.ports[0].timing_mode = PORT_TIMING_NONE;
  config.ports[1].enabled = true;
  config.ports[1].baud_rate = 460800;
  config.ports[1].timing_mode = PORT_TIMING_NONE;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, AcceptsSupportedMixedPortModes) {
  runtime_config_t config = {};
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  config.ports[0].enabled = true;
  config.ports[0].baud_rate = 9600;
  config.ports[0].timing_mode = PORT_TIMING_SYNC;
  config.ports[0].sync_edge_mode = SYNC_EDGE_RISING;

  config.ports[1].enabled = true;
  config.ports[1].baud_rate = 460800;
  config.ports[1].timing_mode = PORT_TIMING_TRIGGER;
  config.ports[1].trigger_period_us = 1000;
  config.ports[1].trigger_pulse_width_us = 50;

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

}  // namespace
