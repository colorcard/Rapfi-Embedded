#ifndef _rp_driver_timer_h_
#define _rp_driver_timer_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 定时器逻辑编号。 */
typedef enum {
  TIMER_1 = 0, /**< TIM1：PWM CH1~CH4，PC0~PC3 */
  TIMER_2,     /**< TIM2：PWM CH1~CH4，PD3/PD4/PD7/PD6 */
  TIMER_3,     /**< TIM3：PWM CH1~CH4，PE2~PE5 */
  TIMER_4,     /**< TIM4：PWM CH3/CH4，PD14/PD15 */
  TIMER_17,    /**< TIM17：10 ms 周期中断（按键扫描） */
  TIMER_NUM
} timer_index_enum;

/**
 * @brief 获取定时器 HAL 句柄。
 * @param index 定时器逻辑编号。
 * @return 句柄指针；编号非法时返回 NULL。
 */
TIM_HandleTypeDef *timer_get_handle(timer_index_enum index);

/**
 * @brief 获取定时器计数时钟频率。
 * @param instance 定时器实例。
 * @return 计数时钟频率，单位 Hz。
 * @note APB 预分频不为 1 时，定时器时钟为对应 PCLK 的 2 倍。
 */
uint32_t timer_clock_hz(TIM_TypeDef *instance);

/**
 * @brief 完成定时器时钟、引脚复用与默认配置初始化。
 * @param index 定时器逻辑编号。
 * @return RP_OK 表示成功，其他值表示失败。
 * @note TIM1~TIM4 会同时配置 PWM 通道；TIM17 只配置基础计数。
 */
rp_status_t timer_hw_init(timer_index_enum index);

#ifdef __cplusplus
}
#endif

#endif /* _rp_driver_timer_h_ */
