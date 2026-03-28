#pragma once

#include "esp_attr.h"

/* Normalizes ESP-IDF IRAM/DRAM attributes behind project-local names. */
#define PLATFORM_ISR_ATTR IRAM_ATTR
/* Marks data that must remain accessible while flash cache is disabled. */
#define PLATFORM_INTERNAL_RAM DRAM_ATTR
