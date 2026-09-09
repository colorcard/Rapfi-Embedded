#ifndef _zf_device_lcd_hw_h_
#define _zf_device_lcd_hw_h_

#include <stdint.h>

#include "zf_common_bsp_config.h"
#include "zf_driver_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_HW_WIDTH LCD_WIDTH
#define LCD_HW_HEIGHT LCD_HEIGHT
#define LCD_SPI_INDEX SPI_1

/**
 * @brief 初始化 ST7789 控制器并开启 LCD 背光
 * @return 0 初始化成功，-1 表示 SPI 通信失败
 * @note 此接口仅供 LCD 用户层调用，应用代码应包含 lcd_user.h
 */
int lcd_hw_init(void);

/**
 * @brief 将 RGB565 像素块写入 LCD 指定区域
 * @param x 目标区域左上角横坐标
 * @param y 目标区域左上角纵坐标
 * @param width 目标区域宽度，单位为像素
 * @param height 目标区域高度，单位为像素
 * @param pixels 按行连续排列的 RGB565 像素数据
 * @return 0 写入成功，-1 表示参数非法或 SPI 通信失败
 * @note 此接口仅由 lcd_update 统一调用
 */
int lcd_hw_write_area_rgb565(uint16_t x, uint16_t y, uint16_t width,
                           uint16_t height, const uint16_t *pixels);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_lcd_hw_h_ */
