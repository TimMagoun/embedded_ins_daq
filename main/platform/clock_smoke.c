#include "clock_smoke.h"

#include <stddef.h>

#include "clock_service.h"
#include "driver/gptimer.h"
#include "esp_check.h"
#include "platform_iram.h"

static const uint32_t kClockIsrSmokePeriodUs = 100000U;

static gptimer_handle_t s_isr_smoke_timer;
static clock_smoke_isr_state_t* s_isr_smoke_state;

/*
 * A dedicated GPTimer alarm exercises the ISR-safe clock path without mixing
 * smoke-only state into the production clock service interface.
 */
static bool PLATFORM_ISR_ATTR clock_smoke_alarm_cb(
    gptimer_handle_t timer, const gptimer_alarm_event_data_t* event_data,
    void* user_ctx) {
  clock_smoke_isr_state_t* state = (clock_smoke_isr_state_t*)user_ctx;

  (void)timer;
  (void)event_data;

  if (state != NULL) {
    state->last_isr_timestamp_us = clock_now_isr();
    state->isr_sample_count += 1U;
  }

  return false;
}

esp_err_t clock_smoke_start_isr(clock_smoke_isr_state_t* state) {
  esp_err_t err;
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,
      .intr_priority = 0,
      .flags =
          {
              .intr_shared = 0,
              .allow_pd = 0,
          },
  };
  gptimer_event_callbacks_t callbacks = {
      .on_alarm = clock_smoke_alarm_cb,
  };
  static gptimer_alarm_config_t alarm_config = {
      .alarm_count = kClockIsrSmokePeriodUs,
      .reload_count = 0,
      .flags =
          {
              .auto_reload_on_alarm = 1,
          },
  };

  if (state == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (s_isr_smoke_timer != NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  state->isr_sample_count = 0U;
  state->last_isr_timestamp_us = 0U;
  s_isr_smoke_state = state;

  ESP_RETURN_ON_ERROR(gptimer_new_timer(&timer_config, &s_isr_smoke_timer),
                      "clock_smoke", "failed to create gptimer smoke timer");

  err = gptimer_register_event_callbacks(s_isr_smoke_timer, &callbacks,
                                         (void*)s_isr_smoke_state);
  if (err != ESP_OK) {
    goto fail;
  }

  err = gptimer_set_alarm_action(s_isr_smoke_timer, &alarm_config);
  if (err != ESP_OK) {
    goto fail;
  }

  err = gptimer_enable(s_isr_smoke_timer);
  if (err != ESP_OK) {
    goto fail;
  }

  err = gptimer_start(s_isr_smoke_timer);
  if (err != ESP_OK) {
    goto fail;
  }

  return ESP_OK;

fail:
  if (s_isr_smoke_timer != NULL) {
    gptimer_disable(s_isr_smoke_timer);
    gptimer_del_timer(s_isr_smoke_timer);
    s_isr_smoke_timer = NULL;
  }
  return err;
}

bool clock_smoke_isr_ready(const clock_smoke_isr_state_t* state,
                           uint32_t minimum_samples) {
  if (state == NULL) {
    return false;
  }

  return state->isr_sample_count >= minimum_samples &&
         state->last_isr_timestamp_us > 0U;
}
