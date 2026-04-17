#include "uart_hal_adapter.h"

#if __has_include("driver/uart.h")

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

esp_err_t uart_hal_adapter_init(uart_hal_adapter_t* adapter,
                                const board_port_profile_t* port, int baud_rate,
                                size_t rx_buffer_size) {
  const uart_config_t config = {
      .baud_rate = baud_rate,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT,
  };
  esp_err_t err;

  if (adapter == NULL || port == NULL || baud_rate <= 0 ||
      rx_buffer_size == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  adapter->uart_controller = port->uart_controller;
  adapter->tx_gpio = port->tx_gpio;
  adapter->rx_gpio = port->rx_gpio;
  adapter->baud_rate = baud_rate;
  adapter->rx_buffer_size = rx_buffer_size;
  adapter->initialized = false;

  err = uart_driver_install((uart_port_t)adapter->uart_controller,
                            rx_buffer_size, 0, 0, NULL, 0);
  if (err != ESP_OK) {
    return err;
  }

  err = uart_param_config((uart_port_t)adapter->uart_controller, &config);
  if (err != ESP_OK) {
    uart_driver_delete((uart_port_t)adapter->uart_controller);
    return err;
  }

  err = uart_set_pin((uart_port_t)adapter->uart_controller, adapter->tx_gpio,
                     adapter->rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    uart_driver_delete((uart_port_t)adapter->uart_controller);
    return err;
  }

  adapter->initialized = true;
  return ESP_OK;
}

esp_err_t uart_hal_adapter_read(uart_hal_adapter_t* adapter, uint8_t* buffer,
                                size_t buffer_capacity, uint32_t timeout_ms,
                                size_t* bytes_read) {
  int read_count;

  if (bytes_read != NULL) {
    *bytes_read = 0U;
  }

  if (adapter == NULL || buffer == NULL || bytes_read == NULL ||
      buffer_capacity == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!adapter->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  read_count = uart_read_bytes((uart_port_t)adapter->uart_controller, buffer,
                               buffer_capacity, pdMS_TO_TICKS(timeout_ms));
  if (read_count < 0) {
    return ESP_ERR_INVALID_STATE;
  }

  *bytes_read = (size_t)read_count;
  return ESP_OK;
}

void uart_hal_adapter_deinit(uart_hal_adapter_t* adapter) {
  if (adapter == NULL || !adapter->initialized) {
    return;
  }

  uart_driver_delete((uart_port_t)adapter->uart_controller);
  adapter->initialized = false;
}

#else

esp_err_t uart_hal_adapter_init(uart_hal_adapter_t* adapter,
                                const board_port_profile_t* port, int baud_rate,
                                size_t rx_buffer_size) {
  (void)adapter;
  (void)port;
  (void)baud_rate;
  (void)rx_buffer_size;
  return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t uart_hal_adapter_read(uart_hal_adapter_t* adapter, uint8_t* buffer,
                                size_t buffer_capacity, uint32_t timeout_ms,
                                size_t* bytes_read) {
  (void)adapter;
  (void)buffer;
  (void)buffer_capacity;
  (void)timeout_ms;
  if (bytes_read != NULL) {
    *bytes_read = 0U;
  }
  return ESP_ERR_NOT_SUPPORTED;
}

void uart_hal_adapter_deinit(uart_hal_adapter_t* adapter) { (void)adapter; }

#endif
