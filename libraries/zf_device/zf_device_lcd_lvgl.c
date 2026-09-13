#include "zf_device_lcd_lvgl.h"

#include "lvgl.h"

#include "zf_device_lcd_hw.h"

/** @brief 每个绘制缓冲的行数（双缓冲，单块约 11KB）。 */
#define LCD_LVGL_DRAW_LINES 20U

/** @brief 双绘制缓冲，实现“渲染下一块”与“DMA 发送当前块”重叠。 */
static uint8_t s_draw_buf1[LCD_WIDTH * LCD_LVGL_DRAW_LINES * 2U];
static uint8_t s_draw_buf2[LCD_WIDTH * LCD_LVGL_DRAW_LINES * 2U];

/** @brief 当前显示对象，供 DMA 完成回调通知 LVGL。 */
static lv_display_t *s_disp;
/** @brief TX DMA 是否仍在进行。 */
static volatile uint8_t s_dma_busy;

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
 * @brief LVGL 刷屏回调：启动一次区域 DMA 传输。
 * @param disp LVGL 显示对象。
 * @param area 需要刷新的矩形区域。
 * @param px_map RGB565 像素数据（LVGL 缓冲，可原地修改）。
 * @return 无。
 */
static void lcd_lvgl_flush(lv_display_t *disp, const lv_area_t *area,
                           uint8_t *px_map)
{
  uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
  uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);
  uint32_t length = (uint32_t)width * (uint32_t)height * 2U;

  s_disp = disp;

  /* 上一块 DMA 未完成前等待；期间 LVGL 已可渲染下一块到另一个缓冲。 */
  while (s_dma_busy != 0U) {
    __WFI();
  }

  lcd_lvgl_swap_rgb565(px_map, length);

  s_dma_busy = 1U;
  if ((lcd_hw_start_area((uint16_t)area->x1, (uint16_t)area->y1, width,
                         height) != 0) ||
      (lcd_hw_send_pixels_dma(px_map, length) != 0)) {
    s_dma_busy = 0U;
    lcd_hw_end_area();
    lv_display_flush_ready(disp);
  }
  /* 成功时在 HAL_SPI_TxCpltCallback 中结束区域并通知 LVGL。 */
}

/**
 * @brief SPI 发送完成回调：结束区域并通知 LVGL 刷屏完成。
 * @param hspi 触发回调的 SPI 句柄。
 * @return 无。
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1) {
    lcd_hw_end_area();
    s_dma_busy = 0U;
    if (s_disp != NULL) {
      lv_display_flush_ready(s_disp);
    }
  }
}

void lcd_lvgl_init(void)
{
  lv_init();
  s_disp = lv_display_create((int32_t)LCD_WIDTH, (int32_t)LCD_HEIGHT);
  if (s_disp == NULL) {
    return;
  }
  lv_display_set_flush_cb(s_disp, lcd_lvgl_flush);
  lv_display_set_buffers(s_disp, s_draw_buf1, s_draw_buf2, sizeof(s_draw_buf1),
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
