#include "gtest/gtest.h"

extern "C" {
#include "ethernet_smoke.h"
}

namespace {

TEST(EthernetSmokeTest, DefaultConfigMatchesDirectLinkPlan) {
  const ethernet_smoke_config_t* config = ethernet_smoke_config();

  ASSERT_NE(config, nullptr);
  EXPECT_STREQ(config->static_ip, "192.168.1.100");
  EXPECT_STREQ(config->netmask, "255.255.255.0");
  EXPECT_STREQ(config->gateway, "192.168.1.1");
}

TEST(EthernetSmokeTest, BoardWiringMatchesEsp32P4Ip101Reference) {
  const ethernet_smoke_config_t* config = ethernet_smoke_config();

  ASSERT_NE(config, nullptr);
  EXPECT_EQ(config->phy_addr, 1);
  EXPECT_EQ(config->phy_reset_gpio, 51);
  EXPECT_EQ(config->mdc_gpio, 31);
  EXPECT_EQ(config->mdio_gpio, 52);
}

TEST(EthernetSmokeTest, ConfigValidates) {
  EXPECT_TRUE(ethernet_smoke_config_valid(ethernet_smoke_config()));
}

TEST(EthernetSmokeTest, RejectsMissingStaticIp) {
  ethernet_smoke_config_t invalid = *ethernet_smoke_config();
  invalid.static_ip = "";

  EXPECT_FALSE(ethernet_smoke_config_valid(&invalid));
}

TEST(EthernetSmokeTest, RejectsMissingGateway) {
  ethernet_smoke_config_t invalid = *ethernet_smoke_config();
  invalid.gateway = nullptr;

  EXPECT_FALSE(ethernet_smoke_config_valid(&invalid));
}

TEST(EthernetSmokeTest, RejectsInvalidPhyAddress) {
  ethernet_smoke_config_t invalid = *ethernet_smoke_config();
  invalid.phy_addr = -1;

  EXPECT_FALSE(ethernet_smoke_config_valid(&invalid));
}

}  // namespace
