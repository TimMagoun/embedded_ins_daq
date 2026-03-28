#pragma once

#include "board_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

void runtime_banner_log_startup(const board_profile_t* profile);
void runtime_banner_log_ready(const char* case_name);
void runtime_banner_start_health_task(void);

#ifdef __cplusplus
}
#endif
