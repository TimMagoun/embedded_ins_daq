#include "ethernet_smoke.h"

static const ethernet_smoke_config_t kEthernetSmokeConfig = {
    .static_ip = "192.168.1.100",
    .netmask = "255.255.255.0",
    .gateway = "192.168.1.1",
    .phy_addr = 1,
    .phy_reset_gpio = 51,
    .mdc_gpio = 31,
    .mdio_gpio = 52,
};

const ethernet_smoke_config_t* ethernet_smoke_config(void) {
  return &kEthernetSmokeConfig;
}

bool ethernet_smoke_config_valid(const ethernet_smoke_config_t* config) {
  if (config == 0) {
    return false;
  }
  if (config->static_ip == 0 || config->static_ip[0] == '\0') {
    return false;
  }
  if (config->netmask == 0 || config->netmask[0] == '\0') {
    return false;
  }
  if (config->gateway == 0 || config->gateway[0] == '\0') {
    return false;
  }
  if (config->phy_addr < 0) {
    return false;
  }
  if (config->phy_reset_gpio < 0) {
    return false;
  }
  if (config->mdc_gpio < 0 || config->mdio_gpio < 0) {
    return false;
  }
  return true;
}
