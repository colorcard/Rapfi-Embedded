#ifndef MENU_USER_H
#define MENU_USER_H

#include <stddef.h>

#include "menu_core.h"

/**
 * @brief 初始化用户菜单页面所使用的业务状态。
 * @return 无。
 */
void menu_user_init(void);

/**
 * @brief 获取由用户配置的完整菜单结构表。
 * @param item_count 用于返回菜单项数量的指针，不可为 NULL。
 * @return 可由菜单核心读取和替换回调的菜单项数组。
 */
menu_item_t *menu_user_get_items(size_t *item_count);

/**
 * @brief 用户自定义功能页面的可编译模板。
 * @param action 菜单核心分发的按键动作。
 * @return 无。
 * @note 默认未挂载；复制或直接挂载到 user_menu_items 即可使用。
 */
void menu_user_page_template(menu_action_enum action);

#endif
