#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  BOARD_PORT_COUNT = 4,
  BOARD_GPIO_UNUSED = -1,
  BOARD_UART_UNUSED = -1,
};

typedef struct {
  const char* name;
  int uart_controller;
  int tx_gpio;
  int rx_gpio;
  int sync_gpio;
  bool enabled;
} board_port_profile_t;

typedef struct {
  const char* profile_name;
  const char* console_path_name;
  int console_uart_controller;
  int console_tx_gpio;
  int console_rx_gpio;
  int sdmmc_slot;
  board_port_profile_t ports[BOARD_PORT_COUNT];
} board_profile_t;

typedef enum {
  BOARD_PROFILE_VALID = 0,
  BOARD_PROFILE_ERR_NULL_PROFILE,
  BOARD_PROFILE_ERR_PROFILE_NAME,
  BOARD_PROFILE_ERR_CONSOLE_UART,
  BOARD_PROFILE_ERR_CONSOLE_PATH_NAME,
  BOARD_PROFILE_ERR_PORT_NAME,
  BOARD_PROFILE_ERR_PORT_UART,
  BOARD_PROFILE_ERR_PORT_PINS_REQUIRED,
  BOARD_PROFILE_ERR_PORT_PINS_DUPLICATED,
  BOARD_PROFILE_ERR_PIN_CONFLICT,
} board_profile_validation_code_t;

typedef struct {
  board_profile_validation_code_t code;
  int port_index;
  int gpio_a;
  int gpio_b;
} board_profile_validation_result_t;

const board_profile_t* board_profile_active(void);
bool board_profile_validate(const board_profile_t* profile,
                            board_profile_validation_result_t* result);
const char* board_profile_validation_message(
    board_profile_validation_code_t code);

#ifdef __cplusplus
}
#endif
