#ifndef _rp_device_lcd_lvgl_h_
#define _rp_device_lcd_lvgl_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 并注册 ST7789 显示驱动。
 * @return 无。
 * @note 需先调用 lcd_hw_init() 完成 ST7789 上电与初始化。
 */
void lcd_lvgl_init(void);

/**
 * @brief LVGL 周期任务：喂时基并处理定时器/刷新。
 * @return 无。
 * @note 在主循环中尽量频繁调用。
 */
void lcd_lvgl_handler(void);

/**
 * @brief 视频模式结束后强制 LVGL 重绘整屏。
 * @return 无。
 */
void lcd_lvgl_resume(void);

#ifdef __cplusplus
}
#endif

#endif /* _rp_device_lcd_lvgl_h_ */
