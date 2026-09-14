#ifndef _rp_device_buzzer_h_
#define _rp_device_buzzer_h_

#include <stdbool.h>

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蜂鸣器 PWM（PA1 = TIM2_CH2），初始静音。
 * @return 无。
 */
void buzzer_init(void);

/**
 * @brief 以指定频率鸣叫（PWM，50% 占空比）。
 * @param frequency_hz 频率，单位 Hz；0 表示不处理。
 * @return 无。
 */
void buzzer_tone(uint32_t frequency_hz);

/**
 * @brief 以默认音调打开蜂鸣器。
 * @return 无。
 */
void buzzer_on(void);

/**
 * @brief 关闭蜂鸣器。
 * @return 无。
 */
void buzzer_off(void);

/**
 * @brief 鸣叫指定时长后自动关闭（阻塞）。
 * @param duration_ms 持续时间，单位毫秒。
 * @return 无。
 */
void buzzer_beep(uint32_t duration_ms);

/**
 * @brief 查询蜂鸣器 PWM 是否初始化成功。
 * @return true 表示可用。
 */
bool buzzer_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* _rp_device_buzzer_h_ */
