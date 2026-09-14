#include "rp_device_lcd_lvgl.h"

#include "lvgl.h"

#include "rp_device_lcd_hw.h"

/** @brief 每个绘制缓冲的行数（条带刷新，单块约 11KB）。 */
#define LCD_LVGL_DRAW_LINES 20U

/** @brief 双绘制缓冲（LVGL 会交替使用）。 */
static uint8_t s_draw_buf1[LCD_WIDTH * LCD_LVGL_DRAW_LINES * 2U];
static uint8_t s_draw_buf2[LCD_WIDTH * LCD_LVGL_DRAW_LINES * 2U];

/**
 * @brief 原地交换 RGB565 字节序（LVGL 小端 -> ST7789 高字节先发）。
 * @param buffer 像素缓冲区。
 * @param byte_length 字节数。
 * @return 无。
 */
static void lcd_lvgl_swap_rgb565(uint8_t *buffer, uint32_t byte_length)
{
  uint32_t i;

  for (i = 0U; (i + 1U) < byte_length; i += 2U) {
    uint8_t temp = buffer[i];
    buffer[i] = buffer[i + 1U];
    buffer[i + 1U] = temp;
  }
}

/**
 * @brief LVGL 刷屏回调：整块 CS + 单次发送到 ST7789。
 * @param disp LVGL 显示对象。
 * @param area 需要刷新的矩形区域。
 * @param px_map RGB565 像素数据。
 * @return 无。
 * @note 42.5MHz 下 280x20 条带约 0.3ms；小区域更新延迟最低。
 */
static void lcd_lvgl_flush(lv_display_t *disp, const lv_area_t *area,
                           uint8_t *px_map)
{
  uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
  uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);
  uint32_t length = (uint32_t)width * (uint32_t)height * 2U;

  lcd_lvgl_swap_rgb565(px_map, length);
  if (lcd_hw_start_area((uint16_t)area->x1, (uint16_t)area->y1, width,
                        height) == 0) {
    (void)spi_write_8bit_array(SPI_1, px_map, (uint16_t)length, 1000U);
    lcd_hw_end_area();
  }
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
  lv_display_set_buffers(disp, s_draw_buf1, s_draw_buf2, sizeof(s_draw_buf1),
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

void lcd_lvgl_resume(void)
{
  lv_obj_invalidate(lv_screen_active());
}
