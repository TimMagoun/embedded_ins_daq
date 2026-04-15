#include "gtest/gtest.h"

extern "C" {
#include "board_ports.h"
}

namespace {

TEST(BoardPortsTest, ExposesFixedHardwareForEachPort) {
  for (size_t i = 0; i < BOARD_PORT_COUNT; ++i) {
    const board_port_t* port = board_port((port_id_t)(i + 1U));

    ASSERT_NE(port, nullptr);
    EXPECT_NE(port->name, nullptr);
    EXPECT_NE(port->name[0], '\0');
    EXPECT_GE(port->uart_controller, 0);
    EXPECT_GE(port->tx_gpio, 0);
    EXPECT_GE(port->rx_gpio, 0);
    EXPECT_GE(port->sync_gpio, 0);
  }
}

TEST(BoardPortsTest, RejectsOutOfRangePortIds) {
  EXPECT_EQ(board_port(PORT_ID_NONE), nullptr);
  EXPECT_EQ(board_port((port_id_t)(BOARD_PORT_COUNT + 1U)), nullptr);
}

TEST(BoardPortsTest, ExposesBoardName) { EXPECT_STREQ(board_name(), "LogDAQ"); }

TEST(BoardPortsTest, ExposesBoardConsolePathName) {
  EXPECT_STREQ(board_console_path_name(), "usb-uart");
}

}  // namespace
