#ifndef MENU_CORE_H
#define MENU_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "menu_key.h"

/** @brief 菜单叶子功能的按键处理回调类型。 */
typedef void (*menu_handler_t)(menu_action_enum action);

/** @brief 功能激活期间的周期回调类型，用于实时刷新观察类页面。 */
typedef void (*menu_poll_t)(void);

typedef struct {
  int16_t id;           /**< 当前菜单项的唯一编号。 */
  int16_t parent_id;    /**< 父菜单编号，-1 表示根级菜单。 */
  const char *name;     /**< 显示在菜单中的零结尾名称。 */
  menu_handler_t handler;  /**< 叶子功能回调，父菜单或无功能项可为 NULL。 */
  menu_poll_t poll;     /**< 功能激活时的周期回调，可为 NULL。 */
} menu_item_t;

/**
 * @brief 初始化菜单状态、按键模块及限频刷新调度器。
 * @param refresh_period_ms 两次 LCD 提交之间的最小间隔，单位 ms；传入 0
 * 时按 1 ms 处理。
 * @return 无。
 * @note 初始化阶段会立即绘制并提交一次根菜单，后续刷新由 menu_process
 * 周期调度。
 */
void menu_init(uint32_t refresh_period_ms);

/**
 * @brief 执行一次菜单系统轮询。
 * @return 无。
 * @note 本函数消费中断扫描产生的按键事件、更新菜单状态，并在刷新周期到达时
 * 提交已请求的画面；应在主循环中持续调用。
 */
void menu_process(void);

/**
 * @brief 将当前菜单状态绘制到帧缓存并提出刷新请求。
 * @return 无。
 * @note 本函数不直接调用 lcd_update，屏幕提交由 menu_process 限频执行。
 */
void menu_draw(void);

/**
 * @brief 标记帧缓存内容已经变化，需要在下一个刷新周期提交。
 * @return 无。
 * @note 自定义菜单功能在修改显示缓存后应调用本函数。
 */
void menu_request_refresh(void);

/**
 * @brief 结束当前叶子功能的交互状态并返回菜单导航状态。
 * @return 无。
 * @note menu_process 会在功能退出后重新绘制所属菜单。
 */
void menu_exit_function(void);

/**
 * @brief 替换指定菜单项的运行时功能回调。
 * @param id 需要修改的菜单项编号。
 * @param handler 新功能回调，可为 NULL 以取消该菜单项的功能。
 * @return true 设置成功；false 未找到对应编号。
 */
bool menu_set_handler(int16_t id, menu_handler_t handler);

/**
 * @brief 获取当前选中的菜单项。
 * @return 当前菜单项的只读指针，生命周期与菜单系统一致。
 */
const menu_item_t *menu_get_current_item(void);

#ifdef __cplusplus
}
#endif

#endif
