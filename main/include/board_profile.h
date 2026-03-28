#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_PORT_COUNT 4
#define BOARD_GPIO_UNUSED (-1)
#define BOARD_UART_UNUSED (-1)

#define BOARD_PORT_PROFILE_DISABLED(name_literal) \
  {                                               \
      .name = (name_literal),                     \
      .uart_controller = BOARD_UART_UNUSED,       \
      .tx_gpio = BOARD_GPIO_UNUSED,               \
      .rx_gpio = BOARD_GPIO_UNUSED,               \
      .sync_gpio = BOARD_GPIO_UNUSED,             \
      .enabled = false,                           \
  }

#define BOARD_PROFILE_VALIDATION_RESULT_INIT \
  {                                          \
      .code = BOARD_PROFILE_VALID,           \
      .port_index = -1,                      \
      .gpio_a = -1,                          \
      .gpio_b = -1,                          \
  }

/* Describes one external sensor/data port on the active board. */
typedef struct {
  /* Human-readable port name used in logs and diagnostics. */
  const char* name;
  /* UART controller number assigned to the port, or BOARD_UART_UNUSED. */
  int uart_controller;
  /* GPIO used for the port TX signal, or BOARD_GPIO_UNUSED. */
  int tx_gpio;
  /* GPIO used for the port RX signal, or BOARD_GPIO_UNUSED. */
  int rx_gpio;
  /* GPIO used for the port sync signal, or BOARD_GPIO_UNUSED. */
  int sync_gpio;
  /* True when the port is wired for the current hardware stage. */
  bool enabled;
} board_port_profile_t;

/* Describes the static board wiring used by this firmware image. */
typedef struct {
  /* Stable profile identifier for logs and validation failures. */
  const char* profile_name;
  /* Human-readable console path name, such as the USB-C UART bridge. */
  const char* console_path_name;
  /* UART controller reserved for console, flashing, and panic output. */
  int console_uart_controller;
  /* Console TX GPIO. */
  int console_tx_gpio;
  /* Console RX GPIO. */
  int console_rx_gpio;
  /* SDMMC slot index used by the onboard TF connector. */
  int sdmmc_slot;
  /* Fixed-size list of logical data ports for the current board. */
  board_port_profile_t ports[BOARD_PORT_COUNT];
} board_profile_t;

/* Enumerates the reason a board profile failed validation. */
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

/* Carries extra context about a validation failure for diagnostics. */
typedef struct {
  board_profile_validation_code_t code;
  int port_index;
  int gpio_a;
  int gpio_b;
} board_profile_validation_result_t;

/* Returns the statically compiled board profile for the active target. */
const board_profile_t* board_profile_active(void);

/* Validates a board profile and optionally fills result with failure details.
 */
bool board_profile_validate(const board_profile_t* profile,
                            board_profile_validation_result_t* result);

/* Returns a short message for the given validation code. */
const char* board_profile_validation_message(
    board_profile_validation_code_t code);

#ifdef __cplusplus
}
#endif
