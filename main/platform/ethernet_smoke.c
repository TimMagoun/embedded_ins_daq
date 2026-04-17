#include "ethernet_smoke.h"

#include <string.h>

#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"

static const char* TAG = "ethernet_smoke";
static const EventBits_t kEthernetLinkBit = BIT0;
static const EventBits_t kEthernetIpv4Bit = BIT1;
static const TickType_t kEthernetReadyTimeoutTicks = pdMS_TO_TICKS(15000);

static EventGroupHandle_t s_event_group;
static esp_eth_handle_t s_eth_handle;
static esp_eth_mac_t* s_mac;
static esp_eth_phy_t* s_phy;
static esp_eth_netif_glue_handle_t s_eth_glue;
static esp_netif_t* s_eth_netif;

static esp_err_t ethernet_smoke_apply_static_ip(
    esp_netif_t* netif, const ethernet_smoke_config_t* config) {
  esp_netif_ip_info_t ip_info;

  memset(&ip_info, 0, sizeof(ip_info));
  if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
    ESP_LOGW(TAG, "DHCP client was not running when static IP was applied");
  }

  ip_info.ip.addr = ipaddr_addr(config->static_ip);
  ip_info.netmask.addr = ipaddr_addr(config->netmask);
  ip_info.gw.addr = ipaddr_addr(config->gateway);
  ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(netif, &ip_info), TAG,
                      "failed to set static IPv4");
  ESP_LOGI(TAG, "Configured static IPv4: ip=%s netmask=%s gateway=%s",
           config->static_ip, config->netmask, config->gateway);
  return ESP_OK;
}

static void ethernet_smoke_eth_event_handler(void* arg,
                                             esp_event_base_t event_base,
                                             int32_t event_id,
                                             void* event_data) {
  const ethernet_smoke_config_t* config = ethernet_smoke_config();
  uint8_t mac_addr[6] = {0};
  esp_eth_handle_t eth_handle = *(esp_eth_handle_t*)event_data;

  (void)arg;
  (void)event_base;

  switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
      ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr));
      ESP_LOGI(TAG, "Ethernet link up: mac=%02x:%02x:%02x:%02x:%02x:%02x",
               mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
               mac_addr[5]);
      ESP_ERROR_CHECK(ethernet_smoke_apply_static_ip(s_eth_netif, config));
      xEventGroupSetBits(s_event_group, kEthernetLinkBit);
      break;
    case ETHERNET_EVENT_DISCONNECTED:
      ESP_LOGW(TAG, "Ethernet link down");
      xEventGroupClearBits(s_event_group, kEthernetLinkBit | kEthernetIpv4Bit);
      break;
    case ETHERNET_EVENT_START:
      ESP_LOGI(TAG, "Ethernet state machine started");
      break;
    case ETHERNET_EVENT_STOP:
      ESP_LOGI(TAG, "Ethernet state machine stopped");
      xEventGroupClearBits(s_event_group, kEthernetLinkBit | kEthernetIpv4Bit);
      break;
    default:
      break;
  }
}

static void ethernet_smoke_ip_event_handler(void* arg,
                                            esp_event_base_t event_base,
                                            int32_t event_id,
                                            void* event_data) {
  ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;

  (void)arg;
  (void)event_base;
  (void)event_id;

  ESP_LOGI(TAG,
           "Ethernet IPv4 ready: ip=" IPSTR " netmask=" IPSTR " gateway=" IPSTR,
           IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.netmask),
           IP2STR(&event->ip_info.gw));
  xEventGroupSetBits(s_event_group, kEthernetIpv4Bit);
}

esp_err_t ethernet_smoke_run(void) {
  const ethernet_smoke_config_t* config = ethernet_smoke_config();
  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
  esp_eth_config_t driver_config;
  EventBits_t bits;

  if (!ethernet_smoke_config_valid(config)) {
    ESP_LOGE(TAG, "Ethernet smoke configuration is invalid");
    return ESP_ERR_INVALID_ARG;
  }

  ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "failed to init esp_netif");
  ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG,
                      "failed to create default event loop");

  s_event_group = xEventGroupCreate();
  if (s_event_group == NULL) {
    return ESP_ERR_NO_MEM;
  }

  s_eth_netif = esp_netif_new(&netif_config);
  if (s_eth_netif == NULL) {
    return ESP_ERR_NO_MEM;
  }

  emac_config.smi_gpio.mdc_num = config->mdc_gpio;
  emac_config.smi_gpio.mdio_num = config->mdio_gpio;
  phy_config.phy_addr = config->phy_addr;
  phy_config.reset_gpio_num = config->phy_reset_gpio;

  s_mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
  if (s_mac == NULL) {
    return ESP_ERR_NO_MEM;
  }
  s_phy = esp_eth_phy_new_ip101(&phy_config);
  if (s_phy == NULL) {
    return ESP_ERR_NO_MEM;
  }

  driver_config = (esp_eth_config_t)ETH_DEFAULT_CONFIG(s_mac, s_phy);
  ESP_RETURN_ON_ERROR(esp_eth_driver_install(&driver_config, &s_eth_handle),
                      TAG, "failed to install Ethernet driver");

  s_eth_glue = esp_eth_new_netif_glue(s_eth_handle);
  if (s_eth_glue == NULL) {
    return ESP_ERR_NO_MEM;
  }

  ESP_RETURN_ON_ERROR(esp_netif_attach(s_eth_netif, s_eth_glue), TAG,
                      "failed to attach Ethernet netif");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                 &ethernet_smoke_eth_event_handler, NULL),
      TAG, "failed to register Ethernet event handler");
  ESP_RETURN_ON_ERROR(
      esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                 &ethernet_smoke_ip_event_handler, NULL),
      TAG, "failed to register IPv4 event handler");
  ESP_RETURN_ON_ERROR(esp_eth_start(s_eth_handle), TAG,
                      "failed to start Ethernet");

  bits = xEventGroupWaitBits(s_event_group, kEthernetLinkBit | kEthernetIpv4Bit,
                             pdFALSE, pdTRUE, kEthernetReadyTimeoutTicks);
  if ((bits & (kEthernetLinkBit | kEthernetIpv4Bit)) !=
      (kEthernetLinkBit | kEthernetIpv4Bit)) {
    ESP_LOGE(TAG,
             "Ethernet smoke timed out waiting for link and IPv4 readiness");
    return ESP_ERR_TIMEOUT;
  }

  return ESP_OK;
}
