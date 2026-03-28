#include <stdint.h>
#include <stdio.h>

#include "clock_probe.h"

typedef struct {
  const uint64_t* values;
  size_t count;
  size_t index;
} fake_clock_t;

static uint64_t fake_clock_read(void* ctx) {
  fake_clock_t* clock = (fake_clock_t*)ctx;
  uint64_t value;

  if (clock->count == 0U) {
    return 0U;
  }

  value = clock->values[clock->index];
  if (clock->index + 1U < clock->count) {
    clock->index += 1U;
  }
  return value;
}

static int expect(int condition, const char* message) {
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    return 0;
  }
  return 1;
}

int main(void) {
  static const uint64_t monotonic_values[] = {10U, 11U, 11U, 25U, 80U, 81U};
  fake_clock_t fake_clock = {
      .values = monotonic_values,
      .count = sizeof(monotonic_values) / sizeof(monotonic_values[0]),
      .index = 0U,
  };
  uint64_t last_sample = 0U;

  if (!expect(clock_probe_monotonic(fake_clock_read, &fake_clock,
                                    fake_clock.count, &last_sample),
              "monotonic fake backend sequence should pass")) {
    return 1;
  }
  if (!expect(last_sample == 81U,
              "last fake clock sample should be reported")) {
    return 1;
  }

  if (!expect(clock_probe_extend_low_word(0x00000000fffffffeULL, 0xffffffffU) ==
                  0x00000000ffffffffULL,
              "low word extension should preserve contiguous values")) {
    return 1;
  }
  if (!expect(clock_probe_extend_low_word(0x00000000ffffffffULL, 0x00000002U) ==
                  0x0000000100000002ULL,
              "low word extension should roll the high word on wrap")) {
    return 1;
  }
  if (!expect(clock_probe_ticks_to_us(2000U, 2000000U) == 1000U,
              "tick-to-us conversion should preserve integer microseconds")) {
    return 1;
  }
  if (!expect(clock_probe_ticks_to_us(0U, 0U) == 0U,
              "zero resolution should return zero")) {
    return 1;
  }

  return 0;
}
