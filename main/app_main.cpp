#include "c_linkage.h"

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#endif

namespace {

#if defined(ESP_PLATFORM)
constexpr char kTag[] = "daq_app";
#endif

}  // namespace

BEGIN_EXTERN_C
void app_main(void) {
#if defined(ESP_PLATFORM)
  ESP_LOGI(kTag, "DAQ boot");
#endif
}
END_EXTERN_C
