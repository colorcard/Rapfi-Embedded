#ifndef _rp_common_clock_h_
#define _rp_common_clock_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 配置系统时钟：HSE 8 MHz + PLL -> SYSCLK 170 MHz。
 * @return 无。
 * @note 必须在 HAL_Init() 之后、任何外设初始化之前调用。
 *       时钟树固定为 HSE 8 MHz -> PLL -> 170 MHz，修改前请先确认 Flash 等待周期与外设时钟。
 */
void clock_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _rp_common_clock_h_ */
