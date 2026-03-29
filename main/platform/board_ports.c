#include "board_ports.h"

static const char* kBoardName = "LogDAQ";
static const char* kBoardConsolePathName = "usb-uart";

static const board_port_t kBoardPorts[BOARD_PORT_COUNT] = {
    {
        .name = "PORT1",
        .uart_controller = 1,
        .tx_gpio = 20,
        .rx_gpio = 21,
        .sync_gpio = 22,
    },
    {
        .name = "PORT2",
        .uart_controller = 2,
        .tx_gpio = 33,
        .rx_gpio = 32,
        .sync_gpio = 36,
    },
    {
        .name = "PORT3",
        .uart_controller = 3,
        .tx_gpio = 2,
        .rx_gpio = 3,
        .sync_gpio = 6,
    },
    {
        .name = "PORT4",
        .uart_controller = 4,
        .tx_gpio = 46,
        .rx_gpio = 47,
        .sync_gpio = 48,
    },
};

const char* board_name(void) { return kBoardName; }

const char* board_console_path_name(void) { return kBoardConsolePathName; }

const board_port_t* board_port(port_id_t port_id) {
  if (port_id < PORT_ID_1 || port_id > PORT_ID_4) {
    return NULL;
  }

  return &kBoardPorts[(size_t)port_id - 1U];
}
