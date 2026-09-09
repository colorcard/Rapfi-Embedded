#ifndef _zf_device_servo_h_
#define _zf_device_servo_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 舵机逻辑编号。 */
typedef enum {
  SERVO_1 = 0,
  SERVO_2,
  SERVO_NUM
} servo_index_enum;

/**
 * @brief 初始化舵机 PWM 输出。
 * @return 当前恒返回 ZF_NOT_READY。
 * @note TODO: 舵机接口对应 TIM4_CH1(PD12)/TIM4_CH2(PD13)，当前被 LCD 的
 *       DC/BL 占用，需硬件取舍后启用 PWM_TIM4_CH1/CH2。
 */
zf_status_t servo_init(void);

/**
 * @brief 设置舵机脉宽。
 * @param servo 舵机编号。
 * @param pulse_us 脉宽，单位微秒（典型 500~2500）。
 * @return 当前恒返回 ZF_NOT_READY。
 */
zf_status_t servo_set_pulse_us(servo_index_enum servo, uint16_t pulse_us);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_servo_h_ */
