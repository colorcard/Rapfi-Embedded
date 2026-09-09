#include "zf_device_servo.h"

#include "zf_driver_gpio.h"
#include "zf_driver_pwm.h"

/** @brief 舵机 PWM 引脚端口。 */
#define SERVO_PORT      GPIOD
/** @brief 舵机 PWM 引脚（PD12/PD13）。 */
#define SERVO_PINS      (GPIO_PIN_12 | GPIO_PIN_13)
/** @brief 舵机 PWM 引脚复用功能。 */
#define SERVO_GPIO_AF   GPIO_AF2_TIM4

/** @brief 舵机标准 PWM 频率，单位 Hz。 */
#define SERVO_FREQUENCY_HZ 50U
/** @brief 舵机最小脉宽，单位微秒。 */
#define SERVO_PULSE_MIN_US 500U
/** @brief 舵机最大脉宽，单位微秒。 */
#define SERVO_PULSE_MAX_US 2500U
/** @brief 舵机最小角度，单位度。 */
#define SERVO_ANGLE_MIN_DEG 0
/** @brief 舵机最大角度，单位度。 */
#define SERVO_ANGLE_MAX_DEG 180

/** @brief 舵机逻辑编号到 PWM 通道的映射。 */
static const pwm_channel_enum s_servo_channel[SERVO_NUM] = {
  PWM_TIM4_CH1, /* SERVO_1: PD12 */
  PWM_TIM4_CH2, /* SERVO_2: PD13 */
};

/**
 * @brief 把脉宽钳位到舵机有效范围。
 * @param pulse_us 原始脉宽，单位微秒。
 * @return 钳位后的脉宽，单位微秒。
 */
static uint16_t servo_clamp_pulse_us(int32_t pulse_us)
{
  if (pulse_us < (int32_t)SERVO_PULSE_MIN_US) {
    return (uint16_t)SERVO_PULSE_MIN_US;
  }
  if (pulse_us > (int32_t)SERVO_PULSE_MAX_US) {
    return (uint16_t)SERVO_PULSE_MAX_US;
  }
  return (uint16_t)pulse_us;
}

zf_status_t servo_init(void)
{
  const gpio_cfg_t gpio_config = {
    SERVO_PORT, (uint16_t)SERVO_PINS, GPIO_MODE_AF_PP, GPIO_NOPULL,
    GPIO_SPEED_FREQ_LOW, SERVO_GPIO_AF
  };
  const pwm_cfg_t pwm_config = {
    SERVO_FREQUENCY_HZ, 0U, true
  };
  zf_status_t status;

  gpio_init(&gpio_config);
  status = pwm_init(PWM_TIM4_CH1, &pwm_config);
  if (status != ZF_OK) {
    return status;
  }
  return pwm_init(PWM_TIM4_CH2, &pwm_config);
}

zf_status_t servo_set_pulse_us(servo_index_enum servo, uint16_t pulse_us)
{
  if ((uint32_t)servo >= (uint32_t)SERVO_NUM) {
    return ZF_INVALID_PARAM;
  }
  return pwm_set_pulse_us(s_servo_channel[servo],
                          servo_clamp_pulse_us((int32_t)pulse_us));
}

zf_status_t servo_set_angle(servo_index_enum servo, int16_t angle_deg)
{
  int32_t angle;
  uint32_t pulse_us;

  if ((uint32_t)servo >= (uint32_t)SERVO_NUM) {
    return ZF_INVALID_PARAM;
  }
  angle = angle_deg;
  if (angle < SERVO_ANGLE_MIN_DEG) {
    angle = SERVO_ANGLE_MIN_DEG;
  } else if (angle > SERVO_ANGLE_MAX_DEG) {
    angle = SERVO_ANGLE_MAX_DEG;
  }
  pulse_us = (uint32_t)SERVO_PULSE_MIN_US +
             (((uint32_t)angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) /
              (uint32_t)SERVO_ANGLE_MAX_DEG);
  return pwm_set_pulse_us(s_servo_channel[servo], pulse_us);
}
