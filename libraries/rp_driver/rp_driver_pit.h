#ifndef _rp_driver_pit_h_
#define _rp_driver_pit_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化周期中断定时器（TIM17）。
 * @param period_ms 中断周期，单位毫秒。
 * @return RP_OK 表示成功，其他值表示失败。
 * @note 中断服务函数中通过 HAL_TIM_PeriodElapsedCallback 派发。
 */
rp_status_t pit_init(uint32_t period_ms);

#ifdef __cplusplus
}
#endif

#endif /* _rp_driver_pit_h_ */
