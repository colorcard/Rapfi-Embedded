#include "rp_driver_pit.h"

#include "rp_driver_timer.h"

/** @brief 定时器自动重装寄存器上限。 */
#define PIT_MAX_PERIOD 0xFFFFU

rp_status_t pit_init(uint32_t period_ms)
{
  TIM_HandleTypeDef *htim;
  uint32_t timer_clock;
  uint64_t ticks;
  uint32_t prescaler;
  uint32_t period;

  if ((period_ms == 0U) || (timer_hw_init(TIMER_17) != RP_OK)) {
    return RP_INVALID_PARAM;
  }
  htim = timer_get_handle(TIMER_17);
  timer_clock = timer_clock_hz(htim->Instance);
  ticks = ((uint64_t)timer_clock * period_ms) / 1000ULL;
  if (ticks == 0U) {
    return RP_INVALID_PARAM;
  }

  for (prescaler = 0U; prescaler <= 0xFFFFU; ++prescaler) {
    period = (uint32_t)(ticks / (uint64_t)(prescaler + 1U));
    if ((period >= 1U) && (period <= (PIT_MAX_PERIOD + 1U))) {
      break;
    }
  }
  if (prescaler > 0xFFFFU) {
    return RP_INVALID_PARAM;
  }

  __HAL_TIM_SET_PRESCALER(htim, prescaler);
  __HAL_TIM_SET_AUTORELOAD(htim, period - 1U);
  __HAL_TIM_SET_COUNTER(htim, 0U);
  return rp_from_hal(HAL_TIM_Base_Start_IT(htim));
}
