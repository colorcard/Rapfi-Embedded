#include "rp_device_buzzer.h"

#include "rp_common_bsp_config.h"
#include "rp_driver_pwm.h"

/** @brief 蜂鸣器所用 PWM 通道：PA1 = TIM2_CH2。 */
#define BUZZER_PWM_CHANNEL      PWM_TIM2_CH2
/** @brief 默认音调频率，单位 Hz。 */
#define BUZZER_DEFAULT_FREQ_HZ  2000U
/** @brief 占空比千分比（越小音量越低；50% 最响）。 */
#define BUZZER_DUTY_PERMILLE    300U

/** @brief PWM 是否已初始化成功。 */
static bool s_buzzer_ready;

void buzzer_init(void)
{
  const pwm_cfg_t cfg = { BUZZER_DEFAULT_FREQ_HZ, 0U, false };

  s_buzzer_ready = false;
  if (pwm_init(BUZZER_PWM_CHANNEL, &cfg) == RP_OK) {
    (void)pwm_set_duty(BUZZER_PWM_CHANNEL, BUZZER_DUTY_PERMILLE);
    s_buzzer_ready = true;
  }
}

void buzzer_tone(uint32_t frequency_hz)
{
  if ((!s_buzzer_ready) || (frequency_hz == 0U)) {
    return;
  }
  (void)pwm_set_frequency(BUZZER_PWM_CHANNEL, frequency_hz);
  (void)pwm_set_duty(BUZZER_PWM_CHANNEL, BUZZER_DUTY_PERMILLE);
  (void)pwm_start(BUZZER_PWM_CHANNEL);
}

void buzzer_on(void)
{
  buzzer_tone(BUZZER_DEFAULT_FREQ_HZ);
}

void buzzer_off(void)
{
  if (s_buzzer_ready) {
    (void)pwm_stop(BUZZER_PWM_CHANNEL);
  }
}

void buzzer_beep(uint32_t duration_ms)
{
  buzzer_on();
  HAL_Delay(duration_ms);
  buzzer_off();
}

bool buzzer_is_ready(void)
{
  return s_buzzer_ready;
}
