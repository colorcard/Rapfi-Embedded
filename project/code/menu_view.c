#include "menu_view.h"

#include <stdarg.h>
#include <stdio.h>

#include "lvgl.h"

/*
 * 高对比深色配色，与旧帧缓存版本保持一致：
 * 背景 #20242C、卡片 #4A525E、文字 #F0F3F7、选中 #F2A33C、选中文字 #241A08。
 */
#define UI_COLOR_BG      lv_color_hex(0x20242CU)
#define UI_COLOR_ITEM    lv_color_hex(0x4A525EU)
#define UI_COLOR_TEXT    lv_color_hex(0xF0F3F7U)
#define UI_COLOR_SEL     lv_color_hex(0xF2A33CU)
#define UI_COLOR_SEL_TXT lv_color_hex(0x241A08U)
#define UI_COLOR_HINT    lv_color_hex(0xA0A6B0U)

/** @brief 页面内的数值标签（最多四个，供各页刷新函数复用）。 */
static lv_obj_t *s_value_label[4];

/**
 * @brief 清空当前屏幕并重置数值标签缓存，作为新页面的画布。
 * @return 当前活动屏幕。
 */
static lv_obj_t *view_reset(void)
{
  lv_obj_t *screen = lv_scr_act();
  uint32_t i;

  lv_obj_clean(screen);
  for (i = 0U; i < 4U; ++i) {
    s_value_label[i] = NULL;
  }
  lv_obj_set_style_bg_color(screen, UI_COLOR_BG, LV_PART_MAIN);
  lv_obj_set_style_text_color(screen, UI_COLOR_TEXT, LV_PART_MAIN);
  return screen;
}

/**
 * @brief 创建页面标题。
 * @param screen 父屏幕。
 * @param text 标题文字。
 * @return 标题标签对象。
 */
static lv_obj_t *view_title(lv_obj_t *screen, const char *text)
{
  lv_obj_t *label = lv_label_create(screen);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, UI_COLOR_TEXT, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 8);
  return label;
}

/**
 * @brief 创建一行左对齐说明文字。
 * @param screen 父屏幕。
 * @param text 文字内容，可为 NULL。
 * @param y 逻辑纵坐标。
 * @return 标签对象。
 */
static lv_obj_t *view_line(lv_obj_t *screen, const char *text, int32_t y)
{
  lv_obj_t *label = lv_label_create(screen);
  if (text != NULL) {
    lv_label_set_text(label, text);
  }
  lv_obj_set_style_text_color(label, UI_COLOR_TEXT, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, y);
  return label;
}

/**
 * @brief 创建底部返回提示。
 * @param screen 父屏幕。
 * @param text 提示文字。
 * @return 无。
 */
static void view_hint(lv_obj_t *screen, const char *text)
{
  lv_obj_t *label = lv_label_create(screen);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, UI_COLOR_HINT, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 14, -8);
}

/**
 * @brief 用 printf 语法更新标签文字（借标准库格式化，避免 LVGL 格式限制）。
 * @param label 目标标签，NULL 时忽略。
 * @param format 格式字符串。
 * @param ... 可变参数。
 * @return 无。
 */
static void view_set_text(lv_obj_t *label, const char *format, ...)
{
  char buffer[48];
  va_list args;

  if ((label == NULL) || (format == NULL)) {
    return;
  }
  va_start(args, format);
  (void)vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  lv_label_set_text(label, buffer);
}

void menu_view_system_navigation_common(const menu_item_t *items,
                                       size_t item_count,
                                       const menu_item_t *current_item)
{
  lv_obj_t *screen;
  lv_obj_t *list;
  size_t i;

  if ((items == NULL) || (current_item == NULL)) {
    return;
  }

  screen = view_reset();
  view_title(screen, "STM32G474 Menu");

  list = lv_obj_create(screen);
  lv_obj_set_size(list, LV_PCT(92), LV_PCT(80));
  lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, -6);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(list, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_row(list, 6, LV_PART_MAIN);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);

  for (i = 0U; i < item_count; ++i) {
    lv_obj_t *button;
    lv_obj_t *label;
    bool selected;

    if (items[i].parent_id != current_item->parent_id) {
      continue;
    }
    selected = (&items[i] == current_item);

    button = lv_btn_create(list);
    lv_obj_set_size(button, LV_PCT(100), 34);
    lv_obj_set_style_bg_color(button, selected ? UI_COLOR_SEL : UI_COLOR_ITEM,
                              LV_PART_MAIN);
    lv_obj_set_style_radius(button, 8, LV_PART_MAIN);

    label = lv_label_create(button);
    lv_label_set_text(label, items[i].name);
    lv_obj_set_style_text_color(label,
                                selected ? UI_COLOR_SEL_TXT : UI_COLOR_TEXT,
                                LV_PART_MAIN);
    lv_obj_center(label);
  }
}

void menu_view_user_function_page_common(const char *title, const char *line1,
                                       const char *line2)
{
  lv_obj_t *screen = view_reset();
  view_title(screen, title);
  (void)view_line(screen, line1, 56);
  (void)view_line(screen, line2, 92);
  view_hint(screen, "BACK: return");
}

void menu_view_user_key_remap_test_8(int32_t value)
{
  lv_obj_t *screen = view_reset();
  view_title(screen, "Key Test");
  s_value_label[0] = view_line(screen, NULL, 56);
  view_set_text(s_value_label[0], "Value: %ld", (long)value);
  (void)view_line(screen, "UP/DOWN, OK reset", 92);
  view_hint(screen, "BACK: return");
}

void menu_view_user_param_view_11_refresh(uint32_t voltage_mv, uint16_t adc_raw,
                                          uint32_t uptime_s)
{
  view_set_text(s_value_label[0], "Volt: %lu mV", (unsigned long)voltage_mv);
  view_set_text(s_value_label[1], "ADC : %u", (unsigned)adc_raw);
  view_set_text(s_value_label[2], "Up  : %lu s", (unsigned long)uptime_s);
}

void menu_view_user_param_view_11(uint32_t voltage_mv, uint16_t adc_raw,
                                  uint32_t uptime_s)
{
  lv_obj_t *screen = view_reset();
  view_title(screen, "Param View");
  s_value_label[0] = view_line(screen, NULL, 56);
  s_value_label[1] = view_line(screen, NULL, 92);
  s_value_label[2] = view_line(screen, NULL, 128);
  view_hint(screen, "BACK: return");
  menu_view_user_param_view_11_refresh(voltage_mv, adc_raw, uptime_s);
}

void menu_view_user_timer_12_refresh(uint32_t seconds, bool running)
{
  uint32_t hours = seconds / 3600U;
  uint32_t minutes = (seconds / 60U) % 60U;
  uint32_t secs = seconds % 60U;

  view_set_text(s_value_label[0], "%02lu:%02lu:%02lu", (unsigned long)hours,
                (unsigned long)minutes, (unsigned long)secs);
  if (s_value_label[1] != NULL) {
    lv_label_set_text(s_value_label[1], running ? "RUN" : "HOLD");
  }
}

void menu_view_user_timer_12(uint32_t seconds, bool running)
{
  lv_obj_t *screen = view_reset();
  view_title(screen, "Timer");
  s_value_label[0] = view_line(screen, NULL, 52);
  s_value_label[1] = view_line(screen, NULL, 92);
  (void)view_line(screen, "OK start/stop", 130);
  (void)view_line(screen, "UP: reset", 160);
  view_hint(screen, "BACK: exit");
  menu_view_user_timer_12_refresh(seconds, running);
}

/**
 * @brief 以 0.1 单位的有符号数值更新标签（如角度、温度）。
 * @param label 目标标签。
 * @param prefix 前缀标签。
 * @param value_d10 数值，单位 0.1。
 * @return 无。
 */
static void view_set_signed_d10(lv_obj_t *label, const char *prefix,
                                int16_t value_d10)
{
  int32_t value = value_d10;

  if (value < 0) {
    view_set_text(label, "%s-%ld.%ld", prefix, (long)((-value) / 10),
                  (long)((-value) % 10));
  } else {
    view_set_text(label, "%s%ld.%ld", prefix, (long)(value / 10),
                  (long)(value % 10));
  }
}

void menu_view_user_imu_angle_9_refresh(const menu_imu_data_t *imu)
{
  if ((imu == NULL) || !imu->valid) {
    view_set_text(s_value_label[0], "IMU read fail");
    return;
  }
  view_set_signed_d10(s_value_label[0], "Pitch: ", imu->pitch_d10);
  view_set_signed_d10(s_value_label[1], "Roll : ", imu->roll_d10);
  view_set_text(s_value_label[2], "Gz   : %d", (int)imu->gyro[2]);
  view_set_signed_d10(s_value_label[3], "Temp : ", imu->temp_d10);
}

void menu_view_user_imu_angle_9(const menu_imu_data_t *imu)
{
  lv_obj_t *screen = view_reset();
  view_title(screen, "IMU Angle");
  s_value_label[0] = view_line(screen, NULL, 52);
  s_value_label[1] = view_line(screen, NULL, 86);
  s_value_label[2] = view_line(screen, NULL, 120);
  s_value_label[3] = view_line(screen, NULL, 154);
  view_hint(screen, "BACK: return");
  menu_view_user_imu_angle_9_refresh(imu);
}
