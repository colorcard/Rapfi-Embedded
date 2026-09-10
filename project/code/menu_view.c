#include "menu_view.h"

#include <string.h>

#include "zf_device_lcd_fonts.h"
#include "zf_device_lcd_user.h"

#define MENU_VIEW_MAX_ROWS       5U   /**< 单页最多显示的菜单行数。 */
#define MENU_VIEW_ITEM_HEIGHT    36U  /**< 每个菜单项占用的垂直像素。 */
#define MENU_VIEW_SAFE_MARGIN    16U  /**< 圆角窗口四周的安全边距。 */

#define MENU_COLOR_BACKGROUND    0xBCCFU /**< 柔和浅茶褐色页面背景。 */
#define MENU_COLOR_ITEM          0x9C0CU /**< 低亮度灰棕色普通卡片。 */
#define MENU_COLOR_TEXT          0x3943U /**< 高对比度深咖啡色普通文字。 */
#define MENU_COLOR_SELECTED      0x7A66U /**< 暖深棕色选中卡片。 */
#define MENU_COLOR_SELECTED_TEXT 0xFF9AU /**< 奶油白色选中文字。 */

/**
 * @brief 计算文字在圆角安全区内允许显示的字符数。
 * @param text 待显示的零结尾字符串。
 * @param x 文字起点的逻辑横坐标。
 * @return 不越过右侧安全边界的字符数量。
 */
static size_t safe_text_length(const char *text, uint16_t x)
{
  uint16_t usable_width; /**< 起点至右侧安全边界的可用像素宽度。 */
  size_t length;         /**< 输入字符串的实际字符数量。 */
  size_t max_chars;      /**< 当前字体和可用宽度允许的字符数量。 */

  if ((text == NULL) ||
      (x >= (uint16_t)(lcd_get_width() - MENU_VIEW_SAFE_MARGIN))) {
    return 0U;
  }

  usable_width = (uint16_t)(lcd_get_width() - MENU_VIEW_SAFE_MARGIN - x);
  max_chars = usable_width / ASCII_Font24.Width;
  length = strlen(text);
  return length < max_chars ? length : max_chars;
}

/**
 * @brief 裁切并绘制一行不会越过右侧安全边界的文字。
 * @param x 文字起点逻辑横坐标。
 * @param y 文字起点逻辑纵坐标。
 * @param text 待绘制字符串。
 * @return 无。
 */
static void draw_safe_string(uint16_t x, uint16_t y, const char *text)
{
  char clipped[32]; /**< 保存裁切后、保证零结尾的临时显示字符串。 */
  /* length 同时受安全边界和临时缓冲区容量限制。 */
  size_t length = safe_text_length(text, x);

  if (length >= sizeof(clipped)) {
    length = sizeof(clipped) - 1U;
  }
  if ((text != NULL) && (length > 0U)) {
    memcpy(clipped, text, length);
  }
  clipped[length] = '\0';
  lcd_fb_draw_string((int16_t)x, (int16_t)y, clipped);
}

/**
 * @brief 使用水平扫描线向帧缓存绘制实心圆角矩形。
 * @param x 矩形左上角逻辑横坐标。
 * @param y 矩形左上角逻辑纵坐标。
 * @param width 矩形宽度，单位像素。
 * @param height 矩形高度，单位像素。
 * @param radius 圆角半径，单位像素。
 * @param color RGB565 填充颜色。
 * @return 无。
 */
static void fill_rounded_rect(uint16_t x, uint16_t y, uint16_t width,
                              uint16_t height, uint16_t radius,
                              uint16_t color)
{
  if ((width == 0U) || (height == 0U)) {
    return;
  }
  if ((uint32_t)radius * 2U > width) {
    radius = width / 2U;
  }
  if ((uint32_t)radius * 2U > height) {
    radius = height / 2U;
  }

  lcd_fb_fill_rect((int16_t)(x + radius), (int16_t)y,
                  (uint16_t)(width - 2U * radius), height, color);
  lcd_fb_fill_rect((int16_t)x, (int16_t)(y + radius), width,
                  (uint16_t)(height - 2U * radius), color);

  for (uint16_t row = 0U; row < radius; ++row) {
    /* dy 表示当前扫描线到圆角圆心的纵向距离。 */
    uint32_t dy = radius - row;
    /* inset 是根据圆方程求得的扫描线左右缩进像素。 */
    uint16_t inset = 0U;
    while (((uint32_t)inset * inset + dy * dy) >
           (uint32_t)radius * radius) {
      ++inset;
    }
    lcd_fb_fill_rect((int16_t)(x + inset), (int16_t)(y + row),
                    (uint16_t)(width - 2U * inset), 1U, color);
    lcd_fb_fill_rect((int16_t)(x + inset),
                    (int16_t)(y + height - 1U - row),
                    (uint16_t)(width - 2U * inset), 1U, color);
  }
}

/**
 * @brief 绘制当前菜单项所在的菜单页面。
 * @param items 完整菜单项数组。
 * @param item_count 数组元素数量。
 * @param current_item 当前选中的菜单项。
 * @return 无。
 */
void menu_view_system_navigation_common(const menu_item_t *items,
                                       size_t item_count,
                                       const menu_item_t *current_item)
{
  const menu_item_t *siblings[16]; /**< 当前层级同级菜单项的临时索引表。 */
  size_t sibling_count = 0U;    /**< 已收集的同级菜单项数量。 */
  size_t selected_index = 0U;   /**< 当前项在同级索引表中的位置。 */
  /* 根据逻辑屏幕高度和圆角边距计算实际可见行数。 */
  uint16_t visible_rows =
      (lcd_get_height() - 2U * MENU_VIEW_SAFE_MARGIN) /
      MENU_VIEW_ITEM_HEIGHT;

  if ((items == NULL) || (current_item == NULL)) {
    return;
  }
  if (visible_rows > MENU_VIEW_MAX_ROWS) {
    visible_rows = MENU_VIEW_MAX_ROWS;
  }
  if (visible_rows == 0U) {
    visible_rows = 1U;
  }

  for (size_t i = 0U; (i < item_count) && (sibling_count < 16U); ++i) {
    if (items[i].parent_id == current_item->parent_id) {
      if (&items[i] == current_item) {
        selected_index = sibling_count;
      }
      siblings[sibling_count++] = &items[i];
    }
  }

  /* page_start 是当前分页在 siblings 中的首项下标。 */
  size_t page_start = 0U;
  if (selected_index >= visible_rows) {
    page_start = selected_index - visible_rows + 1U;
  }

  lcd_fb_clear(MENU_COLOR_BACKGROUND);
  lcd_fb_set_font(&ASCII_Font24);

  for (uint16_t row = 0U;
       (row < visible_rows) && ((page_start + row) < sibling_count);
       ++row) {
    const menu_item_t *item = siblings[page_start + row];
    /* row_y 是当前菜单行在逻辑坐标系中的顶部位置。 */
    uint16_t row_y =
        (uint16_t)(MENU_VIEW_SAFE_MARGIN + row * MENU_VIEW_ITEM_HEIGHT);
    /* 卡片宽度限制在左右各 16 像素的圆角安全区域内。 */
    uint16_t card_width =
        (uint16_t)(lcd_get_width() - 2U * MENU_VIEW_SAFE_MARGIN);

    if (item == current_item) {
      fill_rounded_rect(MENU_VIEW_SAFE_MARGIN, (uint16_t)(row_y + 3U),
                        card_width, 30U, 10U, MENU_COLOR_SELECTED);
      lcd_fb_set_pen_color(MENU_COLOR_SELECTED_TEXT);
      lcd_fb_set_background_color(MENU_COLOR_SELECTED);
    } else {
      fill_rounded_rect(MENU_VIEW_SAFE_MARGIN, (uint16_t)(row_y + 3U),
                        card_width, 30U, 10U, MENU_COLOR_ITEM);
      lcd_fb_set_pen_color(MENU_COLOR_TEXT);
      lcd_fb_set_background_color(MENU_COLOR_ITEM);
    }
    draw_safe_string((uint16_t)(MENU_VIEW_SAFE_MARGIN + 6U),
                     (uint16_t)(row_y + 6U), item->name);
  }
}

/**
 * @brief 绘制包含标题、两行说明和返回提示的通用功能页。
 * @param title 页面标题。
 * @param line1 第一行说明。
 * @param line2 第二行说明。
 * @return 无。
 */
void menu_view_user_function_page_common(const char *title, const char *line1,
                                       const char *line2)
{
  lcd_fb_clear(MENU_COLOR_BACKGROUND);
  lcd_fb_set_font(&ASCII_Font24);
  lcd_fb_set_pen_color(MENU_COLOR_TEXT);
  lcd_fb_set_background_color(MENU_COLOR_BACKGROUND);
  draw_safe_string(MENU_VIEW_SAFE_MARGIN, MENU_VIEW_SAFE_MARGIN, title);
  draw_safe_string(MENU_VIEW_SAFE_MARGIN, 58U, line1);
  draw_safe_string(MENU_VIEW_SAFE_MARGIN, 94U, line2);
  draw_safe_string(
      MENU_VIEW_SAFE_MARGIN,
      (uint16_t)(lcd_get_height() - MENU_VIEW_SAFE_MARGIN -
                 ASCII_Font24.Height),
      "BACK: return");
}

/**
 * @brief 绘制按键测试计数页面。
 * @param value 需要显示的有符号计数值。
 * @return 无。
 */
void menu_view_user_key_remap_test_8(int32_t value)
{
  lcd_fb_clear(MENU_COLOR_BACKGROUND);
  lcd_fb_set_font(&ASCII_Font24);
  lcd_fb_set_pen_color(MENU_COLOR_TEXT);
  lcd_fb_set_background_color(MENU_COLOR_BACKGROUND);
  draw_safe_string(MENU_VIEW_SAFE_MARGIN, MENU_VIEW_SAFE_MARGIN,
                   "key_remap_test");
  lcd_printf(MENU_VIEW_SAFE_MARGIN, 58, "Value: %ld", (long)value);
  draw_safe_string(MENU_VIEW_SAFE_MARGIN, 94U, "UP/DOWN, OK reset");
  draw_safe_string(
      MENU_VIEW_SAFE_MARGIN,
      (uint16_t)(lcd_get_height() - MENU_VIEW_SAFE_MARGIN -
                 ASCII_Font24.Height),
      "BACK: return");
}

void menu_view_user_param_view_11_refresh(uint32_t voltage_mv, uint16_t adc_raw,
                                          uint32_t uptime_s)
{
  /* 先用背景色覆盖数值区，避免新文本比旧文本短时留下残字。 */
  lcd_fb_fill_rect(MENU_VIEW_SAFE_MARGIN, 54, 
                   (uint16_t)(lcd_get_width() - 2U * MENU_VIEW_SAFE_MARGIN),
                   104U, MENU_COLOR_BACKGROUND);

  lcd_printf(MENU_VIEW_SAFE_MARGIN, 58, "Volt:%lu mV", (unsigned long)voltage_mv);
  lcd_printf(MENU_VIEW_SAFE_MARGIN, 94, "ADC :%u", (unsigned int)adc_raw);
  lcd_printf(MENU_VIEW_SAFE_MARGIN, 130, "Up  :%lu s", (unsigned long)uptime_s);
}

void menu_view_user_param_view_11(uint32_t voltage_mv, uint16_t adc_raw,
                                  uint32_t uptime_s)
{
  lcd_fb_clear(MENU_COLOR_BACKGROUND);
  lcd_fb_set_font(&ASCII_Font24);
  lcd_fb_set_pen_color(MENU_COLOR_TEXT);
  lcd_fb_set_background_color(MENU_COLOR_BACKGROUND);
  draw_safe_string(MENU_VIEW_SAFE_MARGIN, MENU_VIEW_SAFE_MARGIN,
                   "param_view");
  menu_view_user_param_view_11_refresh(voltage_mv, adc_raw, uptime_s);
  draw_safe_string(
      MENU_VIEW_SAFE_MARGIN,
      (uint16_t)(lcd_get_height() - MENU_VIEW_SAFE_MARGIN -
                 ASCII_Font24.Height),
      "BACK: return");
}
