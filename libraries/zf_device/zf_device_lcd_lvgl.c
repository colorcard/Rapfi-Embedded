#include "zf_device_lcd_lvgl.h"

#include "lvgl.h"

#include "zf_device_lcd_hw.h"

/** @brief 绘制缓冲的行数（单缓冲条带）。 */
#define LCD_LVGL_DRAW_LINES 20U

/** @brief LVGL 绘制缓冲，以条带方式刷新，节省 RAM。 */
static lv_color_t s_draw_buf[LCD_WIDTH * LCD_LVGL_DRAW_LINES];

/**
 * @brief LVGL 刷屏回调：把脏区数据送到 ST7789。
 * @param drv 显示驱动。
 * @param area 需要刷新的矩形区域。
 * @param color_p RGB565 像素数据。
 * @return 无。
 */
static void lcd_lvgl_flush(lv_disp_drv_t *drv, const lv_area_t *area,
                           lv_color_t *color_p)
{
  uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
  uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);

  (void)lcd_hw_write_area_rgb565((uint16_t)area->x1, (uint16_t)area->y1,
                                 width, height, (const uint16_t *)color_p);
  lv_disp_flush_ready(drv);
}

void lcd_lvgl_init(void)
{
  static lv_disp_draw_buf_t draw_buf;
  static lv_disp_drv_t disp_drv;

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, s_draw_buf, NULL,
                        LCD_WIDTH * LCD_LVGL_DRAW_LINES);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;
  disp_drv.flush_cb = lcd_lvgl_flush;
  disp_drv.draw_buf = &draw_buf;
  (void)lv_disp_drv_register(&disp_drv);
}

void lcd_lvgl_handler(void)
{
  static uint32_t last_tick;
  uint32_t now = HAL_GetTick();

  lv_tick_inc(now - last_tick);
  last_tick = now;
  (void)lv_timer_handler();
}
