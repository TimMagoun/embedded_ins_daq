#include "board_profile.h"

#include <stddef.h>

/* UART0 is the USB-C bridge used during bring-up for flashing and logs. */
static const board_profile_t kEsp32P4NanoProfile = {
    .profile_name = "esp32_p4_nano_rev1_step02",
    .console_path_name = "usb-c-uart0",
    .console_uart_controller = 0,
    .console_tx_gpio = 37,
    .console_rx_gpio = 38,
    .sdmmc_slot = 1,
    .ports =
        {
            {
                .name = "PORT1",
                .uart_controller = 1,
                .tx_gpio = 20,
                .rx_gpio = 21,
                .sync_gpio = 22,
                .enabled = true,
            },
            {
                .name = "PORT2",
                .uart_controller = 2,
                .tx_gpio = 23,
                .rx_gpio = 24,
                .sync_gpio = 25,
                .enabled = true,
            },
            BOARD_PORT_PROFILE_DISABLED("PORT3"),
            BOARD_PORT_PROFILE_DISABLED("PORT4"),
        },
};

static void set_result(board_profile_validation_result_t* result,
                       board_profile_validation_code_t code, int port_index,
                       int gpio_a, int gpio_b) {
  if (result == NULL) {
    return;
  }

  result->code = code;
  result->port_index = port_index;
  result->gpio_a = gpio_a;
  result->gpio_b = gpio_b;
}

static bool gpio_is_used(int gpio) { return gpio >= 0; }

static bool string_has_text(const char* value) {
  return value != NULL && value[0] != '\0';
}

static bool port_pins_are_unique(const board_port_profile_t* port) {
  return port->tx_gpio != port->rx_gpio && port->tx_gpio != port->sync_gpio &&
         port->rx_gpio != port->sync_gpio;
}

/*
 * Check each enabled port pin against the console pins and every other enabled
 * port so the static board description never reuses a GPIO unintentionally.
 */
static bool check_pin_conflict(const board_profile_t* profile, int port_index,
                               int gpio, board_profile_validation_result_t* r) {
  if (!gpio_is_used(gpio)) {
    return true;
  }

  if (gpio == profile->console_tx_gpio || gpio == profile->console_rx_gpio) {
    set_result(r, BOARD_PROFILE_ERR_PIN_CONFLICT, port_index, gpio, gpio);
    return false;
  }

  for (int other_port_index = 0; other_port_index < BOARD_PORT_COUNT;
       ++other_port_index) {
    const board_port_profile_t* other_port = &profile->ports[other_port_index];
    if (!other_port->enabled || other_port_index == port_index) {
      continue;
    }

    if (gpio == other_port->tx_gpio || gpio == other_port->rx_gpio ||
        gpio == other_port->sync_gpio) {
      set_result(r, BOARD_PROFILE_ERR_PIN_CONFLICT, port_index, gpio,
                 other_port_index);
      return false;
    }
  }

  return true;
}

const board_profile_t* board_profile_active(void) {
  return &kEsp32P4NanoProfile;
}

bool board_profile_validate(const board_profile_t* profile,
                            board_profile_validation_result_t* result) {
  if (profile == NULL) {
    set_result(result, BOARD_PROFILE_ERR_NULL_PROFILE, -1, -1, -1);
    return false;
  }

  if (!string_has_text(profile->profile_name)) {
    set_result(result, BOARD_PROFILE_ERR_PROFILE_NAME, -1, -1, -1);
    return false;
  }

  if (!string_has_text(profile->console_path_name)) {
    set_result(result, BOARD_PROFILE_ERR_CONSOLE_PATH_NAME, -1, -1, -1);
    return false;
  }

  if (profile->console_uart_controller < 0) {
    set_result(result, BOARD_PROFILE_ERR_CONSOLE_UART, -1, -1, -1);
    return false;
  }

  for (int i = 0; i < BOARD_PORT_COUNT; ++i) {
    const board_port_profile_t* port = &profile->ports[i];
    if (!string_has_text(port->name)) {
      set_result(result, BOARD_PROFILE_ERR_PORT_NAME, i, -1, -1);
      return false;
    }
    if (!port->enabled) {
      continue;
    }

    if (port->uart_controller < 0) {
      set_result(result, BOARD_PROFILE_ERR_PORT_UART, i, -1, -1);
      return false;
    }

    if (!gpio_is_used(port->tx_gpio) || !gpio_is_used(port->rx_gpio) ||
        !gpio_is_used(port->sync_gpio)) {
      set_result(result, BOARD_PROFILE_ERR_PORT_PINS_REQUIRED, i, port->tx_gpio,
                 port->sync_gpio);
      return false;
    }

    if (!port_pins_are_unique(port)) {
      set_result(result, BOARD_PROFILE_ERR_PORT_PINS_DUPLICATED, i,
                 port->tx_gpio, port->sync_gpio);
      return false;
    }

    if (!check_pin_conflict(profile, i, port->tx_gpio, result) ||
        !check_pin_conflict(profile, i, port->rx_gpio, result) ||
        !check_pin_conflict(profile, i, port->sync_gpio, result)) {
      return false;
    }
  }

  set_result(result, BOARD_PROFILE_VALID, -1, -1, -1);
  return true;
}

const char* board_profile_validation_message(
    board_profile_validation_code_t code) {
  switch (code) {
    case BOARD_PROFILE_VALID:
      return "board profile is valid";
    case BOARD_PROFILE_ERR_NULL_PROFILE:
      return "board profile pointer is null";
    case BOARD_PROFILE_ERR_PROFILE_NAME:
      return "board profile name is required";
    case BOARD_PROFILE_ERR_CONSOLE_UART:
      return "console uart mapping is invalid";
    case BOARD_PROFILE_ERR_CONSOLE_PATH_NAME:
      return "console path name is required";
    case BOARD_PROFILE_ERR_PORT_NAME:
      return "port name is required";
    case BOARD_PROFILE_ERR_PORT_UART:
      return "enabled port must assign a uart controller";
    case BOARD_PROFILE_ERR_PORT_PINS_REQUIRED:
      return "enabled port must define tx, rx, and sync gpio pins";
    case BOARD_PROFILE_ERR_PORT_PINS_DUPLICATED:
      return "port tx, rx, and sync gpio pins must be unique";
    case BOARD_PROFILE_ERR_PIN_CONFLICT:
      return "gpio pin conflicts with console or another enabled port";
    default:
      return "unknown board profile validation result";
  }
}
