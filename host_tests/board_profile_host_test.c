#include <stdio.h>

#include "board_profile.h"

static int expect(int condition, const char* message) {
  if (!condition) {
    fprintf(stderr, "%s\n", message);
    return 0;
  }
  return 1;
}

int main(void) {
  board_profile_validation_result_t result = {0};
  board_profile_t invalid_profile;
  const board_profile_t* active = board_profile_active();

  if (!expect(board_profile_validate(active, &result),
              "active board profile should validate")) {
    return 1;
  }

  invalid_profile = *active;
  invalid_profile.ports[1].sync_gpio = invalid_profile.ports[0].tx_gpio;
  if (!expect(!board_profile_validate(&invalid_profile, &result),
              "conflicting port gpio assignment should fail validation")) {
    return 1;
  }
  if (!expect(result.code == BOARD_PROFILE_ERR_PIN_CONFLICT,
              "conflicting gpio should report pin conflict")) {
    return 1;
  }

  invalid_profile = *active;
  invalid_profile.ports[0].sync_gpio = BOARD_GPIO_UNUSED;
  if (!expect(!board_profile_validate(&invalid_profile, &result),
              "enabled port without sync pin should fail validation")) {
    return 1;
  }
  if (!expect(result.code == BOARD_PROFILE_ERR_PORT_PINS_REQUIRED,
              "missing sync pin should report required pin failure")) {
    return 1;
  }

  return 0;
}
