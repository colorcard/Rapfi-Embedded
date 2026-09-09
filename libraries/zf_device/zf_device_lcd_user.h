#ifndef _zf_device_lcd_user_h_
#define _zf_device_lcd_user_h_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "zf_common_bsp_config.h"
#include "zf_device_lcd_fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_FB_WIDTH LCD_WIDTH
#define LCD_FB_HEIGHT LCD_HEIGHT
#define LCD_RGB565(r, g, b) \
  ((uint16_t)((((uint16_t)(r) & 0xF8U) << 8) | \
              (((uint16_t)(g) & 0xFCU) << 3) | ((uint16_t)(b) >> 3)))

typedef struct {
  uint32_t update_count;
  uint32_t pixels_flushed;
  uint32_t palette_overflows;
  uint16_t palette_size;
  uint16_t dirty_tile_count;
} lcd_stats_t;

/**
 * @brief LCD logical drawing direction.
 */
typedef enum {
  LCD_DIRECTION_PORTRAIT = 0,
  LCD_DIRECTION_LANDSCAPE = 1
} lcd_direction_enum;

/**
 * @brief 初始化 ST7789、调色板和帧缓存，并完成首次全屏刷新
 */
void lcd_init(lcd_direction_enum direction);

/**
 * @brief Return the logical drawing width for the selected direction.
 * @return 240 in portrait mode or 280 in landscape mode.
 */
uint16_t lcd_get_width(void);

/**
 * @brief Return the logical drawing height for the selected direction.
 * @return 280 in portrait mode or 240 in landscape mode.
 */
uint16_t lcd_get_height(void);

/**
 * @brief Return the direction selected during lcd_init.
 * @return Current logical drawing direction.
 */
lcd_direction_enum lcd_get_direction(void);

/**
 * @brief 将帧缓存中的全部脏区统一刷新到 LCD
 * @return 0 全部刷新成功，-1 表示至少一个区域写入失败
 */
int lcd_update(void);

/**
 * @brief 查询当前帧缓存是否存在待刷新的脏区
 * @return true 存在脏区，false 不存在脏区
 */
bool lcd_is_dirty(void);

/**
 * @brief 将整个帧缓存标记为待刷新
 */
void lcd_invalidate_all(void);

/**
 * @brief 获取 LCD 用户层运行统计信息
 * @return 指向只读统计结构的指针
 */
const lcd_stats_t *lcd_get_stats(void);

/**
 * @brief 设置画笔颜色
 * @param rgb565 RGB565 格式颜色
 */
void lcd_fb_set_pen_color(uint16_t rgb565);

/**
 * @brief 设置字符背景颜色
 * @param rgb565 RGB565 格式颜色
 */
void lcd_fb_set_background_color(uint16_t rgb565);

/**
 * @brief 获取当前画笔颜色
 * @return RGB565 格式画笔颜色
 */
uint16_t lcd_fb_get_pen_color(void);

/**
 * @brief 获取当前背景颜色
 * @return RGB565 格式背景颜色
 */
uint16_t lcd_fb_get_background_color(void);

/**
 * @brief 设置 ASCII 字体
 * @param font 字体描述结构，NULL 表示保持当前字体
 */
void lcd_fb_set_font(const pFONT *font);

/**
 * @brief 清空帧缓存
 * @param rgb565 RGB565 格式填充颜色
 */
void lcd_fb_clear(uint16_t rgb565);

/**
 * @brief 绘制像素
 * @param x 横坐标
 * @param y 纵坐标
 * @param rgb565 RGB565 格式颜色
 */
void lcd_fb_draw_pixel(int16_t x, int16_t y, uint16_t rgb565);

/**
 * @brief 绘制线段
 * @param x0 起点横坐标
 * @param y0 起点纵坐标
 * @param x1 终点横坐标
 * @param y1 终点纵坐标
 * @param rgb565 RGB565 格式颜色
 */
void lcd_fb_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t rgb565);

/**
 * @brief 绘制矩形边框
 * @param x 左上角横坐标
 * @param y 左上角纵坐标
 * @param width 宽度
 * @param height 高度
 * @param rgb565 RGB565 格式颜色
 */
void lcd_fb_draw_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t rgb565);

/**
 * @brief 绘制实心矩形
 * @param x 左上角横坐标
 * @param y 左上角纵坐标
 * @param width 宽度
 * @param height 高度
 * @param rgb565 RGB565 格式颜色
 */
void lcd_fb_fill_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t rgb565);

/**
 * @brief 绘制圆形边框
 * @param x0 圆心横坐标
 * @param y0 圆心纵坐标
 * @param radius 半径
 * @param rgb565 RGB565 格式颜色
 */
void lcd_fb_draw_circle(int16_t x0, int16_t y0, uint16_t radius, uint16_t rgb565);

/**
 * @brief 绘制实心圆
 * @param x0 圆心横坐标
 * @param y0 圆心纵坐标
 * @param radius 半径
 * @param rgb565 RGB565 格式颜色
 */
void lcd_fb_fill_circle(int16_t x0, int16_t y0, uint16_t radius, uint16_t rgb565);

/**
 * @brief 绘制字符
 * @param x 左上角横坐标
 * @param y 左上角纵坐标
 * @param ch ASCII 字符
 */
void lcd_fb_draw_char(int16_t x, int16_t y, char ch);

/**
 * @brief 绘制字符串
 * @param x 起始横坐标
 * @param y 起始纵坐标
 * @param text 以空字符结尾的字符串
 */
void lcd_fb_draw_string(int16_t x, int16_t y, const char *text);

/**
 * @brief 绘制 RGB565 图像
 * @param x 左上角横坐标
 * @param y 左上角纵坐标
 * @param width 图像宽度
 * @param height 图像高度
 * @param pixels 按行连续排列的 RGB565 像素数据
 */
void lcd_fb_draw_rgb565(int16_t x, int16_t y, uint16_t width, uint16_t height,
                       const uint16_t *pixels);
                       
/**
 * @brief 绘制 8 位灰度图像
 * @param x 左上角横坐标
 * @param y 左上角纵坐标
 * @param width 图像宽度
 * @param height 图像高度
 * @param pixels 按行连续排列的灰度像素数据
 */
void lcd_fb_draw_gray8(int16_t x, int16_t y, uint16_t width, uint16_t height,
                      const uint8_t *pixels);

/**
 * @brief 按 printf 语法格式化文本并写入帧缓存
 * @param x 文本起始横坐标
 * @param y 文本起始纵坐标
 * @param format printf 风格格式字符串
 * @param ... 与格式字符串对应的可变参数
 * @return 完整格式化结果的字符数，-1 表示格式字符串无效或格式化失败
 */
int lcd_printf(int16_t x, int16_t y, const char *format, ...)
  __attribute__((format(printf, 3, 4)));

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_lcd_user_h_ */
