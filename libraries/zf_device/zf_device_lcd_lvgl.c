#include "zf_device_lcd_lvgl.h"

#include "lvgl.h"

#include "zf_device_lcd_hw.h"

/** @brief 绘制缓冲的行数（单缓冲条带）。 */
#define LCD_LVGL_DRAW_LINES 20U

/** @brief LVGL 绘制缓冲，RGB565 每像素 2 字节。 */
static uint8_t s_draw_buf[LCD_WIDTH * LCD_LVGL_DRAW_LINES * 2U];

/**
 * @brief LVGL 刷屏回调：把脏区数据送到 ST7789。
 * @param disp LVGL 显示对象。
 * @param area 需要刷新的矩形区域。
 * @param px_map RGB565 像素数据。
 * @return 无。
 */
static void lcd_lvgl_flush(lv_display_t *disp, const lv_area_t *area,
                           uint8_t *px_map)
{
  uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
  uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);

  (void)lcd_hw_write_area_rgb565((uint16_t)area->x1, (uint16_t)area->y1,
                                 width, height, (const uint16_t *)px_map);
  lv_display_flush_ready(disp);
}

void lcd_lvgl_init(void)
{
  lv_display_t *disp;

  lv_init();
  disp = lv_display_create((int32_t)LCD_WIDTH, (int32_t)LCD_HEIGHT);
  if (disp == NULL) {
    return;
  }
  lv_display_set_flush_cb(disp, lcd_lvgl_flush);
  lv_display_set_buffers(disp, s_draw_buf, NULL, sizeof(s_draw_buf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void lcd_lvgl_handler(void)
{
  static uint32_t last_tick;
  uint32_t now = HAL_GetTick();

  lv_tick_inc(now - last_tick);
  last_tick = now;
  (void)lv_timer_handler();
}
