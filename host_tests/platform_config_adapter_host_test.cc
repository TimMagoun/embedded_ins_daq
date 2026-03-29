#include "gtest/gtest.h"

extern "C" {
#include "board_profile.h"
#include "platform_config_adapter.h"
}

namespace {

runtime_config_source_t DefaultSource(void) {
  runtime_config_source_t source = {};

  source.port_count = BOARD_PORT_COUNT;
  for (size_t i = 0; i < BOARD_PORT_COUNT; ++i) {
    source.ports[i].enabled = true;
    source.ports[i].baud_rate = 115200;
    source.ports[i].timing_mode = PORT_TIMING_DISABLED;
  }

  return source;
}

TEST(PlatformConfigAdapterTest, MapsBoardCapabilitiesIntoRuntimePortSet) {
  runtime_config_t runtime = {};
  runtime_config_source_t source = DefaultSource();

  ASSERT_EQ(platform_config_adapter_build_runtime(board_profile_active(),
                                                  &source, &runtime),
            ESP_OK);
  EXPECT_EQ(runtime.port_count, BOARD_PORT_COUNT);
  EXPECT_EQ(runtime.ports[0].baud_rate, 115200);
  EXPECT_EQ(runtime.ports[0].uart_port,
            board_profile_active()->ports[0].uart_controller);
}

TEST(PlatformConfigAdapterTest,
     RejectsRuntimeSelectionThatExceedsBoardCapabilities) {
  runtime_config_t runtime = {};
  runtime_config_source_t source = DefaultSource();
  source.port_count = BOARD_PORT_COUNT + 1;

  EXPECT_EQ(platform_config_adapter_build_runtime(board_profile_active(),
                                                  &source, &runtime),
            ESP_ERR_INVALID_ARG);
}

}  // namespace
