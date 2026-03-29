#include "gtest/gtest.h"

extern "C" {
#include "runtime_config.h"
}

namespace {

runtime_config_t ValidConfig(void) {
  runtime_config_t config = {};

  config.port_count = 1;
  config.ports[0].enabled = true;
  config.ports[0].uart_port = 1;
  config.ports[0].timing_mode = PORT_TIMING_TRIGGER_OUTPUT;
  config.ports[0].baud_rate = 921600;
  config.ports[0].trigger_period_us = 100;
  config.ports[0].trigger_pulse_width_us = 10;

  return config;
}

TEST(RuntimeConfigTest, RejectsTriggerPulseWidthNotLessThanPeriod) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].trigger_pulse_width_us = config.ports[0].trigger_period_us;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_TRIGGER_PULSE_WIDTH_INVALID);
}

TEST(RuntimeConfigTest, RejectsPortConfiguredAsSyncAndTriggerTogether) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].enable_sync_input = true;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_TIMING_MODE_CONFLICT);
}

TEST(RuntimeConfigTest, RejectsUnknownTimingMode) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = static_cast<port_timing_mode_t>(99);

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_TIMING_MODE_INVALID);
}

TEST(RuntimeConfigTest, RejectsUnknownSyncEdgeMode) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].timing_mode = PORT_TIMING_SYNC_INPUT;
  config.ports[0].sync_edge_mode = static_cast<sync_edge_mode_t>(99);

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_SYNC_EDGE_MODE_INVALID);
}

TEST(RuntimeConfigTest, RejectsEnabledPortWithoutAssignedUart) {
  runtime_config_t config = ValidConfig();
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;
  config.ports[0].uart_port = BOARD_UART_UNUSED;
  config.ports[0].timing_mode = PORT_TIMING_DISABLED;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_ERR_INVALID_ARG);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_UART_PORT_INVALID);
}

TEST(RuntimeConfigTest, AcceptsReferenceGnssAndImuPortProfileSet) {
  runtime_config_t config = {};
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  config.port_count = 2;
  config.ports[0].enabled = true;
  config.ports[0].uart_port = 1;
  config.ports[0].baud_rate = 9600;
  config.ports[0].timing_mode = PORT_TIMING_DISABLED;
  config.ports[1].enabled = true;
  config.ports[1].uart_port = 2;
  config.ports[1].baud_rate = 921600;
  config.ports[1].timing_mode = PORT_TIMING_DISABLED;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

TEST(RuntimeConfigTest, AcceptsFourIndependentPortModes) {
  runtime_config_t config = {};
  runtime_config_error_t error = RUNTIME_CONFIG_ERROR_NONE;

  config.port_count = 4;

  config.ports[0].enabled = true;
  config.ports[0].uart_port = 1;
  config.ports[0].baud_rate = 9600;
  config.ports[0].timing_mode = PORT_TIMING_SYNC_INPUT;
  config.ports[0].sync_edge_mode = SYNC_EDGE_RISING;

  config.ports[1].enabled = true;
  config.ports[1].uart_port = 2;
  config.ports[1].baud_rate = 921600;
  config.ports[1].timing_mode = PORT_TIMING_TRIGGER_OUTPUT;
  config.ports[1].trigger_period_us = 1000;
  config.ports[1].trigger_pulse_width_us = 50;

  config.ports[2].enabled = true;
  config.ports[2].uart_port = 3;
  config.ports[2].baud_rate = 115200;
  config.ports[2].timing_mode = PORT_TIMING_DISABLED;

  config.ports[3].enabled = true;
  config.ports[3].uart_port = 4;
  config.ports[3].baud_rate = 460800;
  config.ports[3].timing_mode = PORT_TIMING_SYNC_INPUT;
  config.ports[3].sync_edge_mode = SYNC_EDGE_BOTH;

  EXPECT_EQ(runtime_config_validate(&config, &error), ESP_OK);
  EXPECT_EQ(error, RUNTIME_CONFIG_ERROR_NONE);
}

}  // namespace
