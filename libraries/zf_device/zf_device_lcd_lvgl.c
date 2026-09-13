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

/* ------------------------------- 演示界面 ------------------------------- */

static lv_obj_t *s_demo_label;
static lv_obj_t *s_demo_bar;
static lv_obj_t *s_demo_arc;
static uint32_t s_demo_count;

/**
 * @brief 演示界面的周期动画回调。
 * @param timer LVGL 定时器（未使用）。
 * @return 无。
 */
static void lcd_lvgl_demo_timer(lv_timer_t *timer)
{
  (void)timer;
  ++s_demo_count;
  lv_label_set_text_fmt(s_demo_label, "LVGL tick: %lu",
                        (unsigned long)s_demo_count);
  lv_bar_set_value(s_demo_bar, (int32_t)(s_demo_count % 101U), LV_ANIM_ON);
  lv_arc_set_value(s_demo_arc, (int32_t)(s_demo_count % 101U));
}

void lcd_lvgl_demo(void)
{
  lv_obj_t *screen = lv_scr_act();
  lv_obj_t *title;

  lv_obj_set_style_bg_color(screen, lv_color_hex(0x20242C), LV_PART_MAIN);
  lv_obj_set_style_text_color(screen, lv_color_hex(0xF0F3F7), LV_PART_MAIN);

  title = lv_label_create(screen);
  lv_label_set_text(title, "LVGL 8.3.11 / STM32G474");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  s_demo_label = lv_label_create(screen);
  lv_label_set_text(s_demo_label, "LVGL tick: 0");
  lv_obj_align(s_demo_label, LV_ALIGN_TOP_LEFT, 16, 56);

  s_demo_bar = lv_bar_create(screen);
  lv_obj_set_size(s_demo_bar, 200, 18);
  lv_obj_align(s_demo_bar, LV_ALIGN_TOP_MID, 0, 96);
  lv_bar_set_range(s_demo_bar, 0, 100);

  s_demo_arc = lv_arc_create(screen);
  lv_obj_set_size(s_demo_arc, 120, 120);
  lv_obj_align(s_demo_arc, LV_ALIGN_BOTTOM_MID, 0, -16);
  lv_arc_set_range(s_demo_arc, 0, 100);
  lv_arc_set_bg_angles(s_demo_arc, 0, 360);
  lv_arc_set_value(s_demo_arc, 0);

  (void)lv_timer_create(lcd_lvgl_demo_timer, 100, NULL);
}
