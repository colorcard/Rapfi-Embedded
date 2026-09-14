#ifndef _rp_driver_rtc_h_
#define _rp_driver_rtc_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 日历时间（24 小时制）。 */
typedef struct {
  uint8_t hour;   /**< 时，0~23。 */
  uint8_t minute; /**< 分，0~59。 */
  uint8_t second; /**< 秒，0~59。 */
} rtc_time_t;

/**
 * @brief 初始化 RTC，时钟源优先 LSE(32.768 kHz)，无晶振时自动回退 LSI。
 * @return RP_OK 表示成功，其他值表示失败。
 * @note 初始化后时间被置为 2000-01-01 00:00:00，可作为计时基准。
 */
rp_status_t rtc_init(void);

/**
 * @brief 读取当前日历时间。
 * @param time 时间输出指针。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t rtc_get_time(rtc_time_t *time);

/**
 * @brief 读取单调递增的秒计数（含日期，可跨天）。
 * @return 自 2000-01-01 00:00:00 起经过的秒数。
 * @note 适合用作计时器/秒表的基准。
 */
uint32_t rtc_get_seconds(void);

#ifdef __cplusplus
}
#endif

#endif /* _rp_driver_rtc_h_ */
