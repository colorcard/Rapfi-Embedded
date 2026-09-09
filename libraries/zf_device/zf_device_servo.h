#ifndef _zf_device_servo_h_
#define _zf_device_servo_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file zf_device_servo.h
 * @brief 舵机驱动，使用 TIM4_CH1/TIM4_CH2 输出 50 Hz PWM。
 *
 * @warning SERVO_1/SERVO_2 占用 PD12/PD13，而这两脚同时是 LCD 的 DC/BL：
 *          启用舵机即意味着放弃 LCD 的 DC/BL 控制，需按实际硬件取舍。
 */

/** @brief 舵机逻辑编号。 */
typedef enum {
  SERVO_1 = 0, /**< TIM4_CH1 = PD12（与 LCD DC 冲突） */
  SERVO_2,     /**< TIM4_CH2 = PD13（与 LCD BL 冲突） */
  SERVO_NUM
} servo_index_enum;

/**
 * @brief 初始化舵机 PWM 输出。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 把 PD12/PD13 配置为 GPIO_MODE_AF_PP + GPIO_AF2_TIM4，然后以
 *       50 Hz、初始脉宽 0 立即启动两路 PWM。
 * @warning 启用舵机将占用 LCD 的 PD12/PD13，调用后 LCD 的 DC/BL 不可用。
 */
zf_status_t servo_init(void);

/**
 * @brief 设置舵机脉宽。
 * @param servo 舵机编号。
 * @param pulse_us 脉宽，单位微秒；超出 500~2500 时钳位到边界。
 * @return ZF_OK 表示成功，ZF_INVALID_PARAM 表示舵机编号非法。
 */
zf_status_t servo_set_pulse_us(servo_index_enum servo, uint16_t pulse_us);

/**
 * @brief 设置舵机角度。
 * @param servo 舵机编号。
 * @param angle_deg 角度，单位度；0~180 线性映射到 500~2500 us，越界钳位。
 * @return ZF_OK 表示成功，ZF_INVALID_PARAM 表示舵机编号非法。
 */
zf_status_t servo_set_angle(servo_index_enum servo, int16_t angle_deg);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_servo_h_ */
