#pragma once

#include <stddef.h>

#include "runtime_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_PORT_COUNT 4

/* Fixed hardware wiring for one logical port on the only supported board. */
typedef struct {
  const char* name;
  int uart_controller;
  int tx_gpio;
  int rx_gpio;
  int sync_gpio;
} board_port_t;

/* Returns the single supported board name for startup logs. */
const char* board_name(void);

/* Returns the fixed console path name used during bring-up. */
const char* board_console_path_name(void);

/* Returns the fixed hardware mapping for a logical port id. */
const board_port_t* board_port(port_id_t port_id);

#ifdef __cplusplus
}
#endif
