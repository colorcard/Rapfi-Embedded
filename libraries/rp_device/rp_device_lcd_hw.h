#ifndef _rp_device_lcd_hw_h_
#define _rp_device_lcd_hw_h_

#include <stdint.h>

#include "rp_common_bsp_config.h"
#include "rp_driver_spi.h"

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

/**
 * @brief 打开一个显示区域（拉低 CS、设置窗口、切到数据模式）。
 * @param x 区域左上角横坐标。
 * @param y 区域左上角纵坐标。
 * @param width 宽度（像素）。
 * @param height 高度（像素）。
 * @return 0 成功，-1 失败。
 * @note 调用后 CS 保持低电平，需用 lcd_hw_end_area() 结束。
 */
int lcd_hw_start_area(uint16_t x, uint16_t y, uint16_t width, uint16_t height);

/**
 * @brief 以 DMA 方式发送像素数据（非阻塞）。
 * @param pixels 像素缓冲区（RGB565，高字节先发）。
 * @param length 字节数。
 * @return 0 已启动，-1 失败。
 * @note 完成事件在 HAL_SPI_TxCpltCallback 中通知，届时调用 lcd_hw_end_area()。
 */
int lcd_hw_send_pixels_dma(const void *pixels, uint32_t length);

/**
 * @brief 结束当前显示区域（拉高 CS）。
 * @return 无。
 */
void lcd_hw_end_area(void);

#ifdef __cplusplus
}
#endif

#endif /* _rp_device_lcd_hw_h_ */
