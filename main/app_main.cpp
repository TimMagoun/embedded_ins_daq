#include "c_linkage.h"
#include "daq_config.hpp"
#include "esp_log.h"
#include "validate_config.hpp"

namespace {
constexpr char kTag[] = "daq_app";
}  // namespace

BEGIN_EXTERN_C
void app_main(void) {
  ESP_LOGI(kTag, "DAQ boot");

  const daq::FaultCode fault = daq::validate_config(daq::kDefaultConfig);
  if (!daq::is_ok(fault)) {
    ESP_LOGE(kTag, "Config validation failed: origin=%u detail=%u",
             static_cast<unsigned>(fault.origin),
             static_cast<unsigned>(fault.detail));
    return;
  }

  ESP_LOGI(kTag, "Config validation succeeded");
}
END_EXTERN_C
