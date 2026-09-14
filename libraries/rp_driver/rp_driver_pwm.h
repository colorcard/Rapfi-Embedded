#ifndef _rp_driver_pwm_h_
#define _rp_driver_pwm_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 板级 PWM 通道逻辑编号。 */
typedef enum {
  PWM_TIM1_CH1 = 0, /**< PC0 */
  PWM_TIM1_CH2,     /**< PC1 */
  PWM_TIM1_CH3,     /**< PC2 */
  PWM_TIM1_CH4,     /**< PC3 */
  PWM_TIM2_CH1,     /**< PD3 */
  PWM_TIM2_CH2,     /**< PD4 */
  PWM_TIM2_CH3,     /**< PD7 */
  PWM_TIM2_CH4,     /**< PD6 */
  PWM_TIM3_CH1,     /**< PE2 */
  PWM_TIM3_CH2,     /**< PE3 */
  PWM_TIM3_CH3,     /**< PE4 */
  PWM_TIM3_CH4,     /**< PE5 */
  PWM_TIM4_CH1,     /**< PD12（与 LCD DC 冲突） */
  PWM_TIM4_CH2,     /**< PD13（与 LCD BL 冲突） */
  PWM_TIM4_CH3,     /**< PD14 */
  PWM_TIM4_CH4,     /**< PD15 */
  PWM_NUM
} pwm_channel_enum;

/** @brief PWM 初始化参数。 */
typedef struct {
  uint32_t frequency_hz; /**< PWM 频率，单位 Hz。 */
  uint32_t pulse;        /**< 初始比较值（计数值，不是微秒）。 */
  bool start;            /**< true 表示初始化后立即启动输出。 */
} pwm_cfg_t;

/**
 * @brief 初始化 PWM 通道的频率与初始占空比。
 * @param channel PWM 通道。
 * @param cfg 配置参数；传 NULL 使用 bsp_config.h 默认频率、占空比 0 且不启动。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t pwm_init(pwm_channel_enum channel, const pwm_cfg_t *cfg);

/**
 * @brief 重新设置 PWM 频率（会重算预分频与自动重装值）。
 * @param channel PWM 通道。
 * @param frequency_hz 目标频率，单位 Hz。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t pwm_set_frequency(pwm_channel_enum channel, uint32_t frequency_hz);

/**
 * @brief 设置比较值。
 * @param channel PWM 通道。
 * @param pulse 比较值，范围 0~ARR。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t pwm_set_pulse(pwm_channel_enum channel, uint32_t pulse);

/**
 * @brief 设置占空比。
 * @param channel PWM 通道。
 * @param duty_permille 占空比千分比，0~1000。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t pwm_set_duty(pwm_channel_enum channel, uint32_t duty_permille);

/**
 * @brief 以微秒为单位设置脉宽（常用于舵机）。
 * @param channel PWM 通道。
 * @param pulse_us 脉宽，单位微秒。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t pwm_set_pulse_us(pwm_channel_enum channel, uint32_t pulse_us);

/**
 * @brief 启动 PWM 输出。
 * @param channel PWM 通道。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t pwm_start(pwm_channel_enum channel);

/**
 * @brief 停止 PWM 输出。
 * @param channel PWM 通道。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t pwm_stop(pwm_channel_enum channel);

#ifdef __cplusplus
}
#endif

#endif /* _rp_driver_pwm_h_ */
