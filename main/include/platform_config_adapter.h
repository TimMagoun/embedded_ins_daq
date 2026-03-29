#pragma once

#include "board_profile.h"
#include "runtime_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maps board capabilities and operator settings into a runtime contract. */
esp_err_t platform_config_adapter_build_runtime(
    const board_profile_t* board, const runtime_config_source_t* source,
    runtime_config_t* out);

#ifdef __cplusplus
}
#endif
