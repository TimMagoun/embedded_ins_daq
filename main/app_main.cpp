#include "c_linkage.h"
#include "daq_config.hpp"
#include "esp_log.h"
#include "status_fault_hub.hpp"
#include "validate_config.hpp"

namespace {
constexpr char kTag[] = "daq_app";
}  // namespace

BEGIN_EXTERN_C
void app_main(void) {
  ESP_LOGI(kTag, "DAQ boot");

  daq::StatusFaultHub status_hub;
  daq::StatusEvent validation_started{};
  validation_started.origin = daq::StatusOrigin::kConfig;
  validation_started.code = daq::StatusCode::kConfigValidationStarted;
  validation_started.state = daq::State::kInit;
  status_hub.ReportStatus(validation_started);

  const daq::FaultCode fault = daq::validate_config(daq::kDefaultConfig);
  if (!daq::is_ok(fault)) {
    ESP_LOGE(kTag, "Config validation failed: origin=%u detail=%u",
             static_cast<unsigned>(fault.origin),
             static_cast<unsigned>(fault.detail));
    status_hub.ReportFault(fault);
    return;
  }

  daq::StatusEvent validation_succeeded{};
  validation_succeeded.origin = daq::StatusOrigin::kConfig;
  validation_succeeded.code = daq::StatusCode::kConfigValidationSucceeded;
  validation_succeeded.state = daq::State::kReady;
  status_hub.ReportStatus(validation_succeeded);

  ESP_LOGI(kTag, "Config validation succeeded");
}
END_EXTERN_C
