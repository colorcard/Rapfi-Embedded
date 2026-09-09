#ifndef _zf_device_buzzer_h_
#define _zf_device_buzzer_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蜂鸣器并保持关闭。
 * @return 无。
 * @note 当前 PA1 按 GPIO 输出驱动晶体管；若后续确认需要 3 kHz 激励，
 *       再改为定时器 PWM 输出。
 */
void buzzer_init(void);

/**
 * @brief 打开蜂鸣器。
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

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_buzzer_h_ */
