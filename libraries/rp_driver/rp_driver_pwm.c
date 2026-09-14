#include "rp_driver_pwm.h"

#include "rp_common_bsp_config.h"
#include "rp_driver_timer.h"

/** @brief 定时器 16 位自动重装上限。 */
#define PWM_MAX_PERIOD 0xFFFFU

/** @brief PWM 通道的硬件映射。 */
typedef struct {
  timer_index_enum timer;
  uint32_t channel;
} pwm_hw_t;

/** @brief 按逻辑编号排列的 PWM 硬件映射表。 */
static const pwm_hw_t s_pwm[PWM_NUM] = {
  { TIMER_1, TIM_CHANNEL_1 }, /* PWM_TIM1_CH1 */
  { TIMER_1, TIM_CHANNEL_2 }, /* PWM_TIM1_CH2 */
  { TIMER_1, TIM_CHANNEL_3 }, /* PWM_TIM1_CH3 */
  { TIMER_1, TIM_CHANNEL_4 }, /* PWM_TIM1_CH4 */
  { TIMER_2, TIM_CHANNEL_1 }, /* PWM_TIM2_CH1 */
  { TIMER_2, TIM_CHANNEL_2 }, /* PWM_TIM2_CH2 */
  { TIMER_2, TIM_CHANNEL_3 }, /* PWM_TIM2_CH3 */
  { TIMER_2, TIM_CHANNEL_4 }, /* PWM_TIM2_CH4 */
  { TIMER_3, TIM_CHANNEL_1 }, /* PWM_TIM3_CH1 */
  { TIMER_3, TIM_CHANNEL_2 }, /* PWM_TIM3_CH2 */
  { TIMER_3, TIM_CHANNEL_3 }, /* PWM_TIM3_CH3 */
  { TIMER_3, TIM_CHANNEL_4 }, /* PWM_TIM3_CH4 */
  { TIMER_4, TIM_CHANNEL_1 }, /* PWM_TIM4_CH1 */
  { TIMER_4, TIM_CHANNEL_2 }, /* PWM_TIM4_CH2 */
  { TIMER_4, TIM_CHANNEL_3 }, /* PWM_TIM4_CH3 */
  { TIMER_4, TIM_CHANNEL_4 }, /* PWM_TIM4_CH4 */
};

/** @brief 各定时器是否已完成硬件初始化。 */
static bool s_timer_ready[TIMER_NUM];

/**
 * @brief 确保通道所属定时器已经完成初始化。
 * @param channel PWM 通道。
 * @return RP_OK 表示成功，其他值表示失败。
 */
static rp_status_t pwm_ensure_timer(pwm_channel_enum channel)
{
  const pwm_hw_t *hw;

  if ((uint32_t)channel >= (uint32_t)PWM_NUM) {
    return RP_INVALID_PARAM;
  }
  hw = &s_pwm[channel];
  if (!s_timer_ready[hw->timer]) {
    if (timer_hw_init(hw->timer) != RP_OK) {
      return RP_ERROR;
    }
    s_timer_ready[hw->timer] = true;
  }
  return RP_OK;
}

rp_status_t pwm_init(pwm_channel_enum channel, const pwm_cfg_t *cfg)
{
  uint32_t frequency = (cfg != NULL) ? cfg->frequency_hz : PWM_DEFAULT_FREQUENCY_HZ;
  uint32_t pulse = (cfg != NULL) ? cfg->pulse : 0U;
  bool start = (cfg != NULL) ? cfg->start : false;

  if (pwm_ensure_timer(channel) != RP_OK) {
    return RP_INVALID_PARAM;
  }
  if (pwm_set_frequency(channel, frequency) != RP_OK) {
    return RP_INVALID_PARAM;
  }
  if (pwm_set_pulse(channel, pulse) != RP_OK) {
    return RP_INVALID_PARAM;
  }
  if (start) {
    return pwm_start(channel);
  }
  return RP_OK;
}

rp_status_t pwm_set_frequency(pwm_channel_enum channel, uint32_t frequency_hz)
{
  const pwm_hw_t *hw;
  TIM_HandleTypeDef *htim;
  uint32_t timer_clock;
  uint32_t prescaler;
  uint64_t period = 0U;

  if ((pwm_ensure_timer(channel) != RP_OK) || (frequency_hz == 0U)) {
    return RP_INVALID_PARAM;
  }
  hw = &s_pwm[channel];
  htim = timer_get_handle(hw->timer);
  timer_clock = timer_clock_hz(htim->Instance);

  for (prescaler = 0U; prescaler <= 0xFFFFU; ++prescaler) {
    period = (uint64_t)timer_clock / ((uint64_t)(prescaler + 1U) * frequency_hz);
    if ((period >= 1U) && (period <= (PWM_MAX_PERIOD + 1U))) {
      break;
    }
  }
  if (prescaler > 0xFFFFU) {
    return RP_INVALID_PARAM;
  }

  __HAL_TIM_SET_PRESCALER(htim, prescaler);
  __HAL_TIM_SET_AUTORELOAD(htim, (uint32_t)(period - 1U));
  return RP_OK;
}

rp_status_t pwm_set_pulse(pwm_channel_enum channel, uint32_t pulse)
{
  const pwm_hw_t *hw;
  TIM_HandleTypeDef *htim;
  uint32_t autoreload;

  if (pwm_ensure_timer(channel) != RP_OK) {
    return RP_INVALID_PARAM;
  }
  hw = &s_pwm[channel];
  htim = timer_get_handle(hw->timer);
  autoreload = __HAL_TIM_GET_AUTORELOAD(htim);
  if (pulse > autoreload) {
    pulse = autoreload;
  }
  __HAL_TIM_SET_COMPARE(htim, hw->channel, pulse);
  return RP_OK;
}

rp_status_t pwm_set_duty(pwm_channel_enum channel, uint32_t duty_permille)
{
  const pwm_hw_t *hw;
  TIM_HandleTypeDef *htim;
  uint32_t period;
  uint32_t pulse;

  if ((pwm_ensure_timer(channel) != RP_OK) || (duty_permille > 1000U)) {
    return RP_INVALID_PARAM;
  }
  hw = &s_pwm[channel];
  htim = timer_get_handle(hw->timer);
  period = __HAL_TIM_GET_AUTORELOAD(htim) + 1U;
  pulse = (uint32_t)(((uint64_t)period * duty_permille) / 1000ULL);
  __HAL_TIM_SET_COMPARE(htim, hw->channel, pulse);
  return RP_OK;
}

rp_status_t pwm_set_pulse_us(pwm_channel_enum channel, uint32_t pulse_us)
{
  const pwm_hw_t *hw;
  TIM_HandleTypeDef *htim;
  uint32_t timer_clock;
  uint32_t prescaler;
  uint64_t tick_hz;
  uint32_t pulse;

  if (pwm_ensure_timer(channel) != RP_OK) {
    return RP_INVALID_PARAM;
  }
  hw = &s_pwm[channel];
  htim = timer_get_handle(hw->timer);
  timer_clock = timer_clock_hz(htim->Instance);
  prescaler = htim->Instance->PSC;
  tick_hz = (uint64_t)timer_clock / (uint64_t)(prescaler + 1U);
  pulse = (uint32_t)(((uint64_t)pulse_us * tick_hz) / 1000000ULL);
  if (pulse > __HAL_TIM_GET_AUTORELOAD(htim)) {
    pulse = __HAL_TIM_GET_AUTORELOAD(htim);
  }

  __HAL_TIM_SET_COMPARE(htim, hw->channel, pulse);
  return RP_OK;
}

rp_status_t pwm_start(pwm_channel_enum channel)
{
  const pwm_hw_t *hw;

  if (pwm_ensure_timer(channel) != RP_OK) {
    return RP_INVALID_PARAM;
  }
  hw = &s_pwm[channel];
  return rp_from_hal(HAL_TIM_PWM_Start(timer_get_handle(hw->timer), hw->channel));
}

rp_status_t pwm_stop(pwm_channel_enum channel)
{
  const pwm_hw_t *hw;

  if (pwm_ensure_timer(channel) != RP_OK) {
    return RP_INVALID_PARAM;
  }
  hw = &s_pwm[channel];
  return rp_from_hal(HAL_TIM_PWM_Stop(timer_get_handle(hw->timer), hw->channel));
}
