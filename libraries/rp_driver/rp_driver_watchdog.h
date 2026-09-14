#ifndef _rp_driver_watchdog_h_
#define _rp_driver_watchdog_h_

#include "rp_common_bsp.h"
#include "rp_common_bsp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IWDG_DEFAULT_TIMEOUT_MS
#define IWDG_DEFAULT_TIMEOUT_MS 1000U /* 真实值由 bsp_config.h 提供，这里兜底 */
#endif

/**
 * @brief 初始化并使能独立看门狗（IWDG）。
 * @param timeout_ms 期望超时时间，单位毫秒；传 0 时使用 IWDG_DEFAULT_TIMEOUT_MS。
 * @return RP_OK 表示成功，RP_INVALID_PARAM 表示超时超出范围，RP_ERROR 表示 HAL 初始化失败。
 * @note 时钟源为 LSI（约 32000 Hz），实际超时 = 4 * 2^prescaler * (reload + 1) / 32000；
 *       自动选取满足 reload <= 4095 的最小分频以获得最高精度；
 *       使能后不可关闭，必须在超时前周期性调用 iwdg_feed()。
 */
rp_status_t iwdg_init(uint32_t timeout_ms);

/**
 * @brief 喂狗，重装 IWDG 计数器。
 * @return 无。
 * @note 未初始化时直接返回；应在主循环或周期任务中调用，周期需小于超时时间。
 */
void iwdg_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* _rp_driver_watchdog_h_ */
