#ifndef MENU_VIEW_H
#define MENU_VIEW_H

#include <stddef.h>
#include <stdint.h>

#include "menu_core.h"
//命名规范：MenuView_<级别>_<作用>_<菜单ID或Common>，换显示屏请自定义自己的MenuView_System_Navigation_Common
/**
 * @brief 根据菜单模型绘制当前同级菜单页到 LCD 帧缓存。
 * @param items 完整菜单项数组。
 * @param item_count 菜单项数组元素数量。
 * @param current_item 当前选中的菜单项。
 * @return 无。
 * @note 本函数只写帧缓存，不调用 lcd_update。
 */
void menu_view_system_navigation_common(const menu_item_t *items,
                                       size_t item_count,
                                       const menu_item_t *current_item);

/**
 * @brief 绘制通用功能提示页到 LCD 帧缓存。
 * @param title 页面标题。
 * @param line1 第一行说明文字。
 * @param line2 第二行说明文字。
 * @return 无。
 * @note 所有文字都会限制在 16 像素圆角安全区内。
 */
void menu_view_user_function_page_common(const char *title, const char *line1,
                                       const char *line2);

/**
 * @brief 绘制按键映射测试页面到 LCD 帧缓存。
 * @param value 当前测试计数值。
 * @return 无。
 */
void menu_view_user_key_remap_test_8(int32_t value);

/**
 * @brief 整页绘制参数观察页面，包含标题、数值与返回提示。
 * @param voltage_mv 电源电压，单位毫伏。
 * @param adc_raw 电源电压通道的 ADC 原始值。
 * @param uptime_s 运行时间，单位秒。
 * @return 无。
 * @note 进入功能页时调用一次；后续刷新请使用
 * menu_view_user_param_view_11_refresh。
 */
void menu_view_user_param_view_11(uint32_t voltage_mv, uint16_t adc_raw,
                                  uint32_t uptime_s);

/**
 * @brief 只重绘参数观察页面的数值区，避免整屏刷新占用 SPI 带宽。
 * @param voltage_mv 电源电压，单位毫伏。
 * @param adc_raw 电源电压通道的 ADC 原始值。
 * @param uptime_s 运行时间，单位秒。
 * @return 无。
 */
void menu_view_user_param_view_11_refresh(uint32_t voltage_mv, uint16_t adc_raw,
                                          uint32_t uptime_s);

/**
 * @brief 整页绘制 RTC 计时器页面。
 * @param seconds 已计时秒数。
 * @param running true 表示正在计时。
 * @return 无。
 */
void menu_view_user_timer_12(uint32_t seconds, bool running);

/**
 * @brief 只重绘计时器页面的数值与状态，避免整屏刷新。
 * @param seconds 已计时秒数。
 * @param running true 表示正在计时。
 * @return 无。
 */
void menu_view_user_timer_12_refresh(uint32_t seconds, bool running);

#endif
