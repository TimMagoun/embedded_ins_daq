#include "clock_probe.h"

bool clock_probe_monotonic(clock_probe_read_fn_t read_fn, void* ctx,
                           size_t sample_count, uint64_t* last_sample_out) {
  size_t i;
  uint64_t previous = 0;
  uint64_t current = 0;

  if (read_fn == NULL || sample_count == 0U) {
    if (last_sample_out != NULL) {
      *last_sample_out = 0U;
    }
    return false;
  }

  for (i = 0; i < sample_count; ++i) {
    current = read_fn(ctx);
    if (i > 0U && current < previous) {
      if (last_sample_out != NULL) {
        *last_sample_out = current;
      }
      return false;
    }
    previous = current;
  }

  if (last_sample_out != NULL) {
    *last_sample_out = previous;
  }

  return true;
}

uint64_t clock_probe_extend_low_word(uint64_t previous_extended,
                                     uint32_t next_low_word) {
  /* Preserve the previous high word unless the low word wrapped. */
  uint64_t high_word = previous_extended & 0xffffffff00000000ULL;
  uint32_t previous_low_word = (uint32_t)(previous_extended & 0xffffffffULL);

  if (next_low_word < previous_low_word) {
    high_word += 0x100000000ULL;
  }

  return high_word | next_low_word;
}

uint64_t clock_probe_ticks_to_us(uint64_t ticks, uint32_t resolution_hz) {
  if (resolution_hz == 0U) {
    return 0U;
  }

  return (ticks * 1000000ULL) / (uint64_t)resolution_hz;
}
