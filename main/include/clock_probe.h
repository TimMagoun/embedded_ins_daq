#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t (*clock_probe_read_fn_t)(void* ctx);

bool clock_probe_monotonic(clock_probe_read_fn_t read_fn, void* ctx,
                           size_t sample_count, uint64_t* last_sample_out);
uint64_t clock_probe_extend_low_word(uint64_t previous_extended,
                                     uint32_t next_low_word);
uint64_t clock_probe_ticks_to_us(uint64_t ticks, uint32_t resolution_hz);

#ifdef __cplusplus
}
#endif
