#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Static configuration for the direct-link Ethernet smoke path. */
typedef struct {
  /* Static IPv4 address assigned to the ESP. */
  const char* static_ip;
  /* IPv4 netmask for the direct-link subnet. */
  const char* netmask;
  /* IPv4 gateway address expected on the directly attached host. */
  const char* gateway;
  /* IP101 PHY address on the MDIO bus. */
  int phy_addr;
  /* PHY reset GPIO used by the board. */
  int phy_reset_gpio;
  /* MDC GPIO used by the onboard RMII management bus. */
  int mdc_gpio;
  /* MDIO GPIO used by the onboard RMII management bus. */
  int mdio_gpio;
} ethernet_smoke_config_t;

/* Returns the compiled Ethernet smoke configuration. */
const ethernet_smoke_config_t* ethernet_smoke_config(void);

/* Validates the compiled Ethernet smoke configuration. */
bool ethernet_smoke_config_valid(const ethernet_smoke_config_t* config);

#if __has_include("esp_err.h")
#include "esp_err.h"

/* Starts Ethernet, waits for link and static IPv4 readiness, and logs status.
 */
esp_err_t ethernet_smoke_run(void);
#endif

#ifdef __cplusplus
}
#endif
