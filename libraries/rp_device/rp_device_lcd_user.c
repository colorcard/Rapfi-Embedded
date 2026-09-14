#include "rp_device_lcd_user.h"
#include "rp_device_lcd_hw.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LCD_TILE_SIZE       16U
#define LCD_TILE_COLS       ((LCD_FB_WIDTH + LCD_TILE_SIZE - 1U) / LCD_TILE_SIZE)
#define LCD_TILE_ROWS       ((LCD_FB_HEIGHT + LCD_TILE_SIZE - 1U) / LCD_TILE_SIZE)
#define LCD_PALETTE_SIZE    256U
#define LCD_PRINTF_CAPACITY 256U

/* 8-bit indexed framebuffer: 67,200 bytes instead of 134,400-byte RGB565. */
static uint8_t framebuffer[LCD_FB_HEIGHT][LCD_FB_WIDTH];
static uint16_t palette[LCD_PALETTE_SIZE];
static uint16_t palette_count;
static uint16_t dirty_tiles[LCD_TILE_ROWS];
static uint16_t flush_buffer[LCD_FB_WIDTH * LCD_TILE_SIZE];
static uint8_t pen_index;
static uint8_t background_index;
static const pFONT *active_font;
static lcd_stats_t stats;
static lcd_direction_enum active_direction = LCD_DIRECTION_PORTRAIT;

/**
 * @brief Convert logical coordinates to the portrait physical framebuffer.
 * @param x Logical X coordinate.
 * @param y Logical Y coordinate.
 * @param physical_x Resulting physical X coordinate.
 * @param physical_y Resulting physical Y coordinate.
 */
static void logical_to_physical(uint16_t x, uint16_t y,
                                uint16_t *physical_x, uint16_t *physical_y)
{
  if (active_direction == LCD_DIRECTION_LANDSCAPE) {
    *physical_x = y;
    *physical_y = (uint16_t)(LCD_FB_HEIGHT - 1U - x);
  } else {
    *physical_x = x;
    *physical_y = y;
  }
}

/**
 * @brief 返回两个无符号 16 位整数中的较小值
 * @param a 第一个待比较数值
 * @param b 第二个待比较数值
 * @return a 与 b 中的较小值
 */
static uint16_t min_u16(uint16_t a, uint16_t b)
{
  return (a < b) ? a : b;
}

/**
 * @brief 获取或分配指定 RGB565 颜色的调色板索引
 * @param color RGB565 格式颜色
 * @return 对应的 8 位调色板索引
 */
static uint8_t palette_index(uint16_t color)
{
  uint16_t i;
  uint16_t best = 0U;
  uint32_t best_error = UINT32_MAX;

  for (i = 0U; i < palette_count; ++i) {
    if (palette[i] == color) return (uint8_t)i;
  }
  if (palette_count < LCD_PALETTE_SIZE) {
    palette[palette_count] = color;
    return (uint8_t)palette_count++;
  }

  /* Palette is full: choose the nearest RGB565 entry deterministically. */
  for (i = 0U; i < LCD_PALETTE_SIZE; ++i) {
    int32_t dr = (int32_t)((color >> 11) & 0x1FU) - (int32_t)((palette[i] >> 11) & 0x1FU);
    int32_t dg = (int32_t)((color >> 5) & 0x3FU) - (int32_t)((palette[i] >> 5) & 0x3FU);
    int32_t db = (int32_t)(color & 0x1FU) - (int32_t)(palette[i] & 0x1FU);
    uint32_t error = (uint32_t)(dr * dr + dg * dg + db * db);
    if (error < best_error) {
      best_error = error;
      best = i;
    }
  }
  ++stats.palette_overflows;
  return (uint8_t)best;
}

/**
 * @brief 将与指定矩形相交的分块标记为脏区
 * @param x 矩形左上角横坐标
 * @param y 矩形左上角纵坐标
 * @param width 矩形宽度，单位为像素
 * @param height 矩形高度，单位为像素
 */
static void mark_dirty_rect(int32_t x, int32_t y, int32_t width, int32_t height)
{
  int32_t x2;
  int32_t y2;
  uint16_t tx1, tx2, ty1, ty2, ty;
  uint16_t mask;

  if (width <= 0 || height <= 0) return;
  x2 = x + width - 1;
  y2 = y + height - 1;
  if (x2 < 0 || y2 < 0 || x >= (int32_t)LCD_FB_WIDTH || y >= (int32_t)LCD_FB_HEIGHT) return;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x2 >= (int32_t)LCD_FB_WIDTH) x2 = LCD_FB_WIDTH - 1;
  if (y2 >= (int32_t)LCD_FB_HEIGHT) y2 = LCD_FB_HEIGHT - 1;

  tx1 = (uint16_t)x / LCD_TILE_SIZE;
  tx2 = (uint16_t)x2 / LCD_TILE_SIZE;
  ty1 = (uint16_t)y / LCD_TILE_SIZE;
  ty2 = (uint16_t)y2 / LCD_TILE_SIZE;
  mask = (uint16_t)((((uint32_t)1U << (tx2 - tx1 + 1U)) - 1U) << tx1);
  for (ty = ty1; ty <= ty2; ++ty) dirty_tiles[ty] |= mask;
}

/**
 * @brief 向索引帧缓存写入一个像素并标记对应脏区
 * @param x 像素横坐标
 * @param y 像素纵坐标
 * @param index 调色板索引
 */
static void put_index(int16_t x, int16_t y, uint8_t index)
{
  uint16_t physical_x;
  uint16_t physical_y;
  if (x < 0 || y < 0 || x >= (int16_t)lcd_get_width() || y >= (int16_t)lcd_get_height()) return;
  logical_to_physical((uint16_t)x, (uint16_t)y, &physical_x, &physical_y);
  framebuffer[physical_y][physical_x] = index;
  dirty_tiles[physical_y / LCD_TILE_SIZE] |= (uint16_t)(1U << (physical_x / LCD_TILE_SIZE));
}

/**
 * @brief 初始化 LCD 用户层、帧缓存和显示控制器
 */
void lcd_init(lcd_direction_enum direction)
{
  active_direction = (direction == LCD_DIRECTION_LANDSCAPE)
                     ? LCD_DIRECTION_LANDSCAPE : LCD_DIRECTION_PORTRAIT;
  memset(&stats, 0, sizeof(stats));
  palette_count = 0U;
  background_index = palette_index(0x0000U);
  pen_index = palette_index(0xFFFFU);
  active_font = &ASCII_Font16;
  memset(framebuffer, background_index, sizeof(framebuffer));
  (void)lcd_hw_init();
  lcd_invalidate_all();
  (void)lcd_update();
}

uint16_t lcd_get_width(void)
{
  return (active_direction == LCD_DIRECTION_LANDSCAPE) ? LCD_FB_HEIGHT : LCD_FB_WIDTH;
}

uint16_t lcd_get_height(void)
{
  return (active_direction == LCD_DIRECTION_LANDSCAPE) ? LCD_FB_WIDTH : LCD_FB_HEIGHT;
}

lcd_direction_enum lcd_get_direction(void)
{
  return active_direction;
}

/**
 * @brief 查询当前帧缓存是否存在待刷新的脏区
 * @return true 存在脏区，false 不存在脏区
 */
bool lcd_is_dirty(void)
{
  uint16_t row;
  for (row = 0U; row < LCD_TILE_ROWS; ++row) {
    if (dirty_tiles[row] != 0U) return true;
  }
  return false;
}

/**
 * @brief 将整个帧缓存标记为待刷新
 */
void lcd_invalidate_all(void)
{
  uint16_t row;
  const uint16_t mask = (uint16_t)((1UL << LCD_TILE_COLS) - 1UL);
  for (row = 0U; row < LCD_TILE_ROWS; ++row) dirty_tiles[row] = mask;
}

/**
 * @brief 获取 LCD 用户层运行统计信息
 * @return 指向只读统计结构的指针
 */
const lcd_stats_t *lcd_get_stats(void)
{
  stats.palette_size = palette_count;
  return &stats;
}

/**
 * @brief 将帧缓存中的全部脏区统一刷新到 LCD
 * @return 0 全部刷新成功，-1 表示至少一个区域写入失败
 */
int lcd_update(void)
{
  uint16_t tile_y;
  int result = 0;

  stats.dirty_tile_count = 0U;
  for (tile_y = 0U; tile_y < LCD_TILE_ROWS; ++tile_y) {
    uint16_t pending = dirty_tiles[tile_y];
    uint16_t tile_x = 0U;
    while (tile_x < LCD_TILE_COLS) {
      uint16_t start_tile, end_tile, x, y, width, height, row, col;
      uint16_t run_mask;
      if ((pending & (uint16_t)(1U << tile_x)) == 0U) {
        ++tile_x;
        continue;
      }
      start_tile = tile_x;
      while (tile_x + 1U < LCD_TILE_COLS &&
             (pending & (uint16_t)(1U << (tile_x + 1U))) != 0U) ++tile_x;
      end_tile = tile_x;
      x = start_tile * LCD_TILE_SIZE;
      y = tile_y * LCD_TILE_SIZE;
      width = min_u16((end_tile - start_tile + 1U) * LCD_TILE_SIZE, LCD_FB_WIDTH - x);
      height = min_u16(LCD_TILE_SIZE, LCD_FB_HEIGHT - y);

      for (row = 0U; row < height; ++row) {
        for (col = 0U; col < width; ++col) {
          flush_buffer[row * width + col] = palette[framebuffer[y + row][x + col]];
        }
      }
      run_mask = (uint16_t)((((uint32_t)1U << (end_tile - start_tile + 1U)) - 1U) << start_tile);
      if (lcd_hw_write_area_rgb565(x, y, width, height, flush_buffer) == 0) {
        dirty_tiles[tile_y] &= (uint16_t)~run_mask;
        stats.pixels_flushed += (uint32_t)width * height;
        stats.dirty_tile_count += end_tile - start_tile + 1U;
      } else {
        result = -1;
      }
      ++tile_x;
    }
  }
  ++stats.update_count;
  stats.palette_size = palette_count;
  return result;
}

/**
 * @brief 设置后续文字和图形绘制使用的画笔颜色
 * @param color RGB565 格式颜色
 */
void lcd_fb_set_pen_color(uint16_t color) { pen_index = palette_index(color); }

/**
 * @brief 设置字符空白像素使用的背景颜色
 * @param color RGB565 格式颜色
 */
void lcd_fb_set_background_color(uint16_t color) { background_index = palette_index(color); }

/**
 * @brief 获取当前画笔颜色
 * @return RGB565 格式画笔颜色
 */
uint16_t lcd_fb_get_pen_color(void) { return palette[pen_index]; }

/**
 * @brief 获取当前背景颜色
 * @return RGB565 格式背景颜色
 */
uint16_t lcd_fb_get_background_color(void) { return palette[background_index]; }

/**
 * @brief 设置后续字符绘制使用的 ASCII 字体
 * @param font 字体描述结构，NULL 表示保持当前字体
 */
void lcd_fb_set_font(const pFONT *font) { if (font != NULL) active_font = font; }

/**
 * @brief 使用指定颜色清空整个帧缓存
 * @param color RGB565 格式填充颜色
 */
void lcd_fb_clear(uint16_t color)
{
  uint8_t index = palette_index(color);
  memset(framebuffer, index, sizeof(framebuffer));
  background_index = index;
  lcd_invalidate_all();
}

/**
 * @brief 在帧缓存中绘制一个像素
 * @param x 像素横坐标
 * @param y 像素纵坐标
 * @param color RGB565 格式颜色
 */
void lcd_fb_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
  put_index(x, y, palette_index(color));
}

/**
 * @brief 在帧缓存中绘制一条线段
 * @param x0 起点横坐标
 * @param y0 起点纵坐标
 * @param x1 终点横坐标
 * @param y1 终点纵坐标
 * @param color RGB565 格式颜色
 */
void lcd_fb_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  int16_t dx = (int16_t)((x1 > x0) ? x1 - x0 : x0 - x1);
  int16_t sx = (x0 < x1) ? 1 : -1;
  int16_t dy = (int16_t)-((y1 > y0) ? y1 - y0 : y0 - y1);
  int16_t sy = (y0 < y1) ? 1 : -1;
  int16_t error = dx + dy;
  uint8_t index = palette_index(color);
  for (;;) {
    put_index(x0, y0, index);
    if (x0 == x1 && y0 == y1) break;
    if ((int16_t)(2 * error) >= dy) { error = (int16_t)(error + dy); x0 = (int16_t)(x0 + sx); }
    if ((int16_t)(2 * error) <= dx) { error = (int16_t)(error + dx); y0 = (int16_t)(y0 + sy); }
  }
}

/**
 * @brief 在帧缓存中绘制实心矩形
 * @param x 矩形左上角横坐标
 * @param y 矩形左上角纵坐标
 * @param width 矩形宽度，单位为像素
 * @param height 矩形高度，单位为像素
 * @param color RGB565 格式填充颜色
 */
void lcd_fb_fill_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t color)
{
  int32_t x1 = x, y1 = y, x2 = (int32_t)x + width, y2 = (int32_t)y + height;
  int32_t row, column;
  uint8_t index;
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > (int32_t)lcd_get_width()) x2 = lcd_get_width();
  if (y2 > (int32_t)lcd_get_height()) y2 = lcd_get_height();
  if (x1 >= x2 || y1 >= y2) return;
  index = palette_index(color);
  if (active_direction == LCD_DIRECTION_PORTRAIT) {
    for (row = y1; row < y2; ++row) memset(&framebuffer[row][x1], index, (size_t)(x2 - x1));
    mark_dirty_rect(x1, y1, x2 - x1, y2 - y1);
  } else {
    for (row = y1; row < y2; ++row)
      for (column = x1; column < x2; ++column)
        put_index((int16_t)column, (int16_t)row, index);
  }
}

/**
 * @brief 在帧缓存中绘制矩形边框
 * @param x 矩形左上角横坐标
 * @param y 矩形左上角纵坐标
 * @param width 矩形宽度，单位为像素
 * @param height 矩形高度，单位为像素
 * @param color RGB565 格式边框颜色
 */
void lcd_fb_draw_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t color)
{
  if (width == 0U || height == 0U) return;
  lcd_fb_draw_line(x, y, (int16_t)(x + width - 1U), y, color);
  lcd_fb_draw_line(x, (int16_t)(y + height - 1U), (int16_t)(x + width - 1U), (int16_t)(y + height - 1U), color);
  lcd_fb_draw_line(x, y, x, (int16_t)(y + height - 1U), color);
  lcd_fb_draw_line((int16_t)(x + width - 1U), y, (int16_t)(x + width - 1U), (int16_t)(y + height - 1U), color);
}

/**
 * @brief 在帧缓存中绘制圆形边框
 * @param x0 圆心横坐标
 * @param y0 圆心纵坐标
 * @param radius 圆半径，单位为像素
 * @param color RGB565 格式边框颜色
 */
void lcd_fb_draw_circle(int16_t x0, int16_t y0, uint16_t radius, uint16_t color)
{
  int16_t x = (int16_t)radius, y = 0, error = 1 - (int16_t)radius;
  uint8_t index = palette_index(color);
  while (x >= y) {
    put_index((int16_t)(x0 + x), (int16_t)(y0 + y), index); put_index((int16_t)(x0 + y), (int16_t)(y0 + x), index);
    put_index((int16_t)(x0 - y), (int16_t)(y0 + x), index); put_index((int16_t)(x0 - x), (int16_t)(y0 + y), index);
    put_index((int16_t)(x0 - x), (int16_t)(y0 - y), index); put_index((int16_t)(x0 - y), (int16_t)(y0 - x), index);
    put_index((int16_t)(x0 + y), (int16_t)(y0 - x), index); put_index((int16_t)(x0 + x), (int16_t)(y0 - y), index);
    ++y;
    if (error < 0) error = (int16_t)(error + 2 * y + 1);
    else { --x; error = (int16_t)(error + 2 * (y - x) + 1); }
  }
}

/**
 * @brief 在帧缓存中绘制实心圆
 * @param x0 圆心横坐标
 * @param y0 圆心纵坐标
 * @param radius 圆半径，单位为像素
 * @param color RGB565 格式填充颜色
 */
void lcd_fb_fill_circle(int16_t x0, int16_t y0, uint16_t radius, uint16_t color)
{
  int16_t y;
  int32_t rr = (int32_t)radius * radius;
  for (y = -(int16_t)radius; y <= (int16_t)radius; ++y) {
    int16_t x = 0;
    while ((int32_t)(x + 1) * (x + 1) + (int32_t)y * y <= rr) ++x;
    lcd_fb_fill_rect((int16_t)(x0 - x), (int16_t)(y0 + y), (uint16_t)(2 * x + 1), 1U, color);
  }
}

/**
 * @brief 使用当前字体和颜色在帧缓存中绘制单个字符
 * @param x 字符左上角横坐标
 * @param y 字符左上角纵坐标
 * @param ch 要绘制的 ASCII 字符
 */
void lcd_fb_draw_char(int16_t x, int16_t y, char ch)
{
  uint16_t row, column;
  uint16_t bytes_per_row;
  uint16_t glyph;
  if (active_font == NULL) return;
  if ((unsigned char)ch < 32U || (unsigned char)ch > 126U) ch = '?';
  glyph = (uint16_t)((unsigned char)ch - 32U);
  bytes_per_row = (uint16_t)((active_font->Width + 7U) / 8U);
  if ((uint32_t)bytes_per_row * active_font->Height > active_font->Sizes) return;

  for (row = 0U; row < active_font->Height; ++row) {
    const uint8_t *row_data = &active_font->pTable[
      (uint32_t)glyph * active_font->Sizes + (uint32_t)row * bytes_per_row];
    for (column = 0U; column < active_font->Width; ++column) {
      uint8_t bits = row_data[column / 8U];
      put_index((int16_t)(x + column), (int16_t)(y + row),
                ((bits >> (column % 8U)) & 1U) ? pen_index : background_index);
    }
  }
}

/**
 * @brief 使用当前字体和颜色在帧缓存中绘制字符串
 * @param x 字符串起始横坐标
 * @param y 字符串起始纵坐标
 * @param text 以空字符结尾的字符串
 */
void lcd_fb_draw_string(int16_t x, int16_t y, const char *text)
{
  int16_t cursor_x = x;
  if (text == NULL || active_font == NULL) return;
  while (*text != '\0') {
    if (*text == '\n') { cursor_x = x; y = (int16_t)(y + active_font->Height); ++text; continue; }
    if (cursor_x + (int16_t)active_font->Width > (int16_t)lcd_get_width()) {
      cursor_x = x;
      y = (int16_t)(y + active_font->Height);
    }
    if (y >= (int16_t)lcd_get_height()) break;
    lcd_fb_draw_char(cursor_x, y, *text++);
    cursor_x = (int16_t)(cursor_x + active_font->Width);
  }
}

/**
 * @brief 将 RGB565 图像写入帧缓存
 * @param x 图像左上角横坐标
 * @param y 图像左上角纵坐标
 * @param width 图像宽度，单位为像素
 * @param height 图像高度，单位为像素
 * @param pixels 按行连续排列的 RGB565 像素数据
 */
void lcd_fb_draw_rgb565(int16_t x, int16_t y, uint16_t width, uint16_t height, const uint16_t *pixels)
{
  uint16_t row, col;
  if (pixels == NULL) return;
  for (row = 0U; row < height; ++row)
    for (col = 0U; col < width; ++col)
      lcd_fb_draw_pixel((int16_t)(x + col), (int16_t)(y + row), pixels[row * width + col]);
}

/**
 * @brief 将 8 位灰度图像写入帧缓存
 * @param x 图像左上角横坐标
 * @param y 图像左上角纵坐标
 * @param width 图像宽度，单位为像素
 * @param height 图像高度，单位为像素
 * @param pixels 按行连续排列的 8 位灰度像素数据
 */
void lcd_fb_draw_gray8(int16_t x, int16_t y, uint16_t width, uint16_t height, const uint8_t *pixels)
{
  uint16_t row, col;
  if (pixels == NULL) return;
  for (row = 0U; row < height; ++row)
    for (col = 0U; col < width; ++col) {
      uint8_t gray = pixels[row * width + col];
      lcd_fb_draw_pixel((int16_t)(x + col), (int16_t)(y + row), LCD_RGB565(gray, gray, gray));
    }
}

/**
 * @brief 按 printf 语法格式化文本并写入帧缓存
 * @param x 文本起始横坐标
 * @param y 文本起始纵坐标
 * @param format printf 风格格式字符串
 * @param ... 与格式字符串对应的可变参数
 * @return 完整格式化结果的字符数，-1 表示格式字符串无效或格式化失败
 */
int lcd_printf(int16_t x, int16_t y, const char *format, ...)
{
  char text[LCD_PRINTF_CAPACITY];
  int result;
  va_list args;
  if (format == NULL) return -1;
  va_start(args, format);
  result = vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  lcd_fb_draw_string(x, y, text);
  return result;
}
