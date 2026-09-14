#ifndef _rp_device_led_h_
#define _rp_device_led_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 板级 LED 逻辑编号。 */
typedef enum {
  LED_STATUS = 0, /**< PA0 板载 LED。 */
  LED_NUM
} led_index_enum;

/**
 * @brief 初始化 LED 并保持熄灭。
 * @return 无。
 * @note 本函数负责把 PA0 配置为推挽输出。
 */
void led_init(void);

/**
 * @brief 点亮指定 LED。
 * @param led LED 逻辑编号。
 * @return 无。
 */
void led_on(led_index_enum led);

/**
 * @brief 熄灭指定 LED。
 * @param led LED 逻辑编号。
 * @return 无。
 */
void led_off(led_index_enum led);

/**
 * @brief 翻转指定 LED。
 * @param led LED 逻辑编号。
 * @return 无。
 */
void led_toggle(led_index_enum led);

#ifdef __cplusplus
}
#endif

#endif /* _rp_device_led_h_ */
