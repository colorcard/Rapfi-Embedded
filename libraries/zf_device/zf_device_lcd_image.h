#ifndef _zf_device_lcd_image_h_
#define _zf_device_lcd_image_h_

#include <stdint.h>

/**
 * @file lcd_image.h
 * @brief 声明由外部取模数据生成的 RGB565 启动图片资源。
 * @note 图片已经离线缩放为适合 240x280 LCD 的尺寸；运行时不执行缩放。
 */

/** @brief 启动图片宽度，单位像素。 */
#define STARTUP_IMAGE_WIDTH  240U
/** @brief 启动图片高度，单位像素。 */
#define STARTUP_IMAGE_HEIGHT 257U

/**
 * @brief 按从左到右、从上到下顺序保存的 RGB565 图片像素。
 * @note 可传给 lcd_fb_draw_rgb565 绘制；数组包含
 * STARTUP_IMAGE_WIDTH * STARTUP_IMAGE_HEIGHT 个 uint16_t 像素。
 */
extern const uint16_t startup_image[STARTUP_IMAGE_WIDTH * STARTUP_IMAGE_HEIGHT];

#endif /* _zf_device_lcd_image_h_ */
