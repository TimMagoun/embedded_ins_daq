#include "sync_hal_adapter.h"

#if __has_include("driver/gpio.h")

#include "board_profile.h"
#include "driver/gpio.h"

static gpio_int_type_t interrupt_type_for_mode(sync_edge_mode_t edge_mode) {
  switch (edge_mode) {
    case SYNC_EDGE_RISING:
      return GPIO_INTR_POSEDGE;
    case SYNC_EDGE_FALLING:
      return GPIO_INTR_NEGEDGE;
    case SYNC_EDGE_BOTH:
      return GPIO_INTR_ANYEDGE;
    case SYNC_EDGE_NONE:
    default:
      return GPIO_INTR_DISABLE;
  }
}

static const board_port_profile_t* sync_port_profile_for_id(port_id_t port_id) {
  const board_profile_t* board = board_profile_active();
  const size_t port_index = (size_t)port_id - 1U;

  if (board == NULL || port_id == PORT_ID_NONE ||
      port_index >= BOARD_PORT_COUNT) {
    return NULL;
  }

  return &board->ports[port_index];
}

esp_err_t sync_hal_adapter_configure_input(port_id_t port_id,
                                           sync_edge_mode_t edge_mode) {
  const board_port_profile_t* port = sync_port_profile_for_id(port_id);
  gpio_config_t config = {};

  if (port == NULL || !port->enabled || port->sync_gpio == BOARD_GPIO_UNUSED ||
      edge_mode == SYNC_EDGE_NONE) {
    return ESP_ERR_INVALID_ARG;
  }

  config.pin_bit_mask = 1ULL << (uint64_t)port->sync_gpio;
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = interrupt_type_for_mode(edge_mode);
  return gpio_config(&config);
}

void sync_hal_adapter_deinit_input(port_id_t port_id) {
  const board_port_profile_t* port = sync_port_profile_for_id(port_id);

  if (port == NULL || port->sync_gpio == BOARD_GPIO_UNUSED) {
    return;
  }

  (void)gpio_set_intr_type((gpio_num_t)port->sync_gpio, GPIO_INTR_DISABLE);
}

#else

esp_err_t sync_hal_adapter_configure_input(port_id_t port_id,
                                           sync_edge_mode_t edge_mode) {
  (void)port_id;
  (void)edge_mode;
  return ESP_ERR_NOT_SUPPORTED;
}

void sync_hal_adapter_deinit_input(port_id_t port_id) { (void)port_id; }

#endif
