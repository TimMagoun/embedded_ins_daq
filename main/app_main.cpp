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
  ESP_LOGD(kTag,
           "Validating default config: enabled_uarts=%u mask=0x%02x chunk=%zu "
           "idle_gap_us=%u record_q=%zu writer_q=%zu sd_block=%zu "
           "trigger_mask=0x%02x sync_mask=0x%02x",
           static_cast<unsigned>(daq::kDefaultConfig.enabled_uart_count),
           static_cast<unsigned>(daq::kDefaultConfig.enabled_uart_mask),
           daq::kDefaultConfig.uart_chunk_size_bytes,
           static_cast<unsigned>(daq::kDefaultConfig.uart_idle_gap_us),
           daq::kDefaultConfig.record_queue_capacity,
           daq::kDefaultConfig.writer_queue_capacity,
           daq::kDefaultConfig.sd_block_size_bytes,
           static_cast<unsigned>(daq::kDefaultConfig.enabled_trigger_mask),
           static_cast<unsigned>(daq::kDefaultConfig.enabled_sync_mask));

  const daq::FaultCode fault = daq::validate_config(daq::kDefaultConfig);
  if (!daq::is_ok(fault)) {
    ESP_LOGE(kTag, "Config validation failed: origin=%u detail=%u",
             static_cast<unsigned>(fault.origin),
             static_cast<unsigned>(fault.detail));
    return;
  }

  ESP_LOGI(kTag, "Config validation succeeded");
  ESP_LOGD(kTag, "Boot path complete; control-plane startup can continue");
}
END_EXTERN_C
