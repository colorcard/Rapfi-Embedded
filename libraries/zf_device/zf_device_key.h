#ifndef _zf_device_key_h_
#define _zf_device_key_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 板级按键逻辑编号（与原理图 KEY1~KEY4 对应）。 */
typedef enum {
  KEY_1 = 0, /**< PA4 */
  KEY_2,     /**< PA5 */
  KEY_3,     /**< PA6 */
  KEY_4,     /**< PA7 */
  KEY_NUM
} key_index_enum;

/**
 * @brief 初始化按键状态。
 * @return 无。
 * @note 引脚复用与上拉配置由 CubeMX 的 MX_GPIO_Init() 完成。
 */
void key_init(void);

/**
 * @brief 读取按键是否处于按下状态。
 * @param key 按键逻辑编号。
 * @return true 表示按下，false 表示松开或编号非法。
 * @note 有效电平由 bsp_config.h 的 KEY_ACTIVE_LOW 决定。
 */
bool key_is_pressed(key_index_enum key);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_key_h_ */
