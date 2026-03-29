#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board_profile.h"

#if __has_include("esp_err.h")
#include "esp_err.h"
#else
#ifndef RUNTIME_ESP_ERR_COMPAT_DEFINED
#define RUNTIME_ESP_ERR_COMPAT_DEFINED
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#endif
#ifndef ESP_ERR_NOT_SUPPORTED
#define ESP_ERR_NOT_SUPPORTED 0x106
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Wraps the device UART driver behind a small project-local contract. */
typedef struct {
  int uart_controller;
  int tx_gpio;
  int rx_gpio;
  int baud_rate;
  size_t rx_buffer_size;
  bool initialized;
} uart_hal_adapter_t;

/* Configures the selected board UART for one-port bring-up capture. */
esp_err_t uart_hal_adapter_init(uart_hal_adapter_t* adapter,
                                const board_port_profile_t* port, int baud_rate,
                                size_t rx_buffer_size);

/* Attempts to read up to buffer_capacity bytes from the configured UART. */
esp_err_t uart_hal_adapter_read(uart_hal_adapter_t* adapter, uint8_t* buffer,
                                size_t buffer_capacity, uint32_t timeout_ms,
                                size_t* bytes_read);

/* Tears down the UART driver if it was installed. */
void uart_hal_adapter_deinit(uart_hal_adapter_t* adapter);

#ifdef __cplusplus
}
#endif
