#include "menu_user.h"

#include "zf_device_lcd_user.h"
#include "zf_device_power.h"
#include "zf_driver_adc.h"
#include "menu_view.h"

static void menu_user_key_remap_test(menu_action_enum action);
static void menu_user_placeholder(menu_action_enum action);
static void menu_user_param_view(menu_action_enum action);
static void menu_user_param_view_poll(void);

/** @brief 按键映射测试页面显示的可增减测试值。 */
static int32_t key_test_value;
/** @brief 用户页面模板演示用的状态变量，可替换为实际业务数据。 */
static int32_t template_value;
/** @brief 参数观察页上一次刷新时的 HAL 毫秒时基，用于限频。 */
static uint32_t param_view_last_tick;

/*
 * 菜单层级规划：
 *   顶层（parent_id = -1）直接列出所有功能页，按 OK 进入、BACK 返回，
 *   不再设置 "Main Menu"/"modeX" 之类的中间容器，避免层级过深。
 *
 * 新增页面：在数组末尾追加 {唯一ID, -1, "清晰标题", 按键回调, 周期回调或NULL}。
 * 命名建议：标题为简短英文（LCD 仅支持 ASCII），一眼能看出功能。
 */
static menu_item_t user_menu_items[] = {
    {11, -1, "Param View", menu_user_param_view, menu_user_param_view_poll},
    {8,  -1, "Key Test",   menu_user_key_remap_test, NULL},
    {9,  -1, "IMU Angle",  menu_user_placeholder, NULL},
    {10, -1, "Motor Cal",  menu_user_placeholder, NULL},
    /*
     * 挂载模板页面示例：
     * {12, -1, "User Page", menu_user_page_template, NULL},
     */
};

/**
 * @brief 初始化用户功能页面中的可变业务数据。
 * @return 无。
 */
void menu_user_init(void)
{
  key_test_value = 0;
  template_value = 0;
  param_view_last_tick = 0U;
}

/**
 * @brief 返回用户定义的菜单配置表及其元素数量。
 * @param item_count 用于接收菜单项数量的有效指针。
 * @return user_menu_items 数组首地址。
 */
menu_item_t *menu_user_get_items(size_t *item_count)
{
  if (item_count != NULL) {
    *item_count = sizeof(user_menu_items) / sizeof(user_menu_items[0]);
  }
  return user_menu_items;
}

/**
 * @brief 演示用户自定义菜单页面的标准按键映射和刷新流程。
 * @param action 菜单核心传入的短按或长按动作。
 * @return 无。
 * @note 使用方法：
 * 1. 在 user_menu_items 中增加 `{唯一ID, 父ID, "页面名",
 * menu_user_page_template}`；
 * 2. 将 template_value 替换为用户自己的页面状态；
 * 3. 按需要修改 switch 中每个按键的业务动作；
 * 4. 将下方通用绘制替换为 MenuView_User_作用_菜单ID 函数；
 * 5. 画面变化后保留 menu_request_refresh，返回时保留
 * menu_exit_function。
 */
void menu_user_page_template(menu_action_enum action)
{
  /* changed 用于避免无效按键引起不必要的帧缓存重绘。 */
  bool changed = false;

  switch (action) {
    case MENU_ACTION_UP:
    case MENU_ACTION_UP_LONG:
      /* 示例：向上键增加参数。 */
      ++template_value;
      changed = true;
      break;

    case MENU_ACTION_DOWN:
    case MENU_ACTION_DOWN_LONG:
      /* 示例：向下键减小参数。 */
      --template_value;
      changed = true;
      break;

    case MENU_ACTION_OK:
    case MENU_ACTION_OK_LONG:
      /* 示例：确认键将参数恢复为默认值。 */
      template_value = 0;
      changed = true;
      break;

    case MENU_ACTION_BACK:
    case MENU_ACTION_BACK_LONG:
      /* 返回键必须退出功能状态，核心随后自动重绘父菜单。 */
      menu_exit_function();
      return;

    default:
      break;
  }

  if (changed) {
    /*
     * 简单页面可以直接组合通用 View 和 LCD 绘制 API；
     * 复杂页面建议在 menu_view.c 中新增
     * MenuView_User_作用_菜单ID。
     * 此处只写帧缓存，不直接调用 lcd_update。
     */
    menu_view_user_function_page_common("User Template",
                                      "UP/DOWN: change",
                                      "OK: reset");
    lcd_printf(16, 130, "Value: %ld", (long)template_value);
    menu_request_refresh();
  }
}

/**
 * @brief 处理按键映射测试功能并重绘该功能页面。
 * @param action 本次需要处理的菜单按键动作。
 * @return 无。
 */
static void menu_user_key_remap_test(menu_action_enum action)
{
  switch (action) {
    case MENU_ACTION_UP:
    case MENU_ACTION_UP_LONG:
      ++key_test_value;
      break;
    case MENU_ACTION_DOWN:
    case MENU_ACTION_DOWN_LONG:
      --key_test_value;
      break;
    case MENU_ACTION_OK:
    case MENU_ACTION_OK_LONG:
      key_test_value = 0;
      break;
    case MENU_ACTION_BACK:
    case MENU_ACTION_BACK_LONG:
      menu_exit_function();
      return;
    default:
      break;
  }

  menu_view_user_key_remap_test_8(key_test_value);
  menu_request_refresh();
}

/**
 * @brief 显示尚未接入具体业务逻辑的用户占位页面。
 * @param action 本次按键动作；返回动作退出当前功能页。可在该函数中自由映射定义功能
 * @return 无。
 */
static void menu_user_placeholder(menu_action_enum action)
{
  //默认保留的返回键位，每个回调函数中必须有该功能，可以只启用一个
  if ((action == MENU_ACTION_BACK) || (action == MENU_ACTION_BACK_LONG)) {
    menu_exit_function();
    return;
  }

  //需要自定义页面绘制函数可在view文件中定义随后在此调用
  menu_view_user_function_page_common(menu_get_current_item()->name,
                                    "Callback not installed",
                                    "Edit menu_user.c");
  menu_request_refresh();//申请刷屏，为了便于维护建议不要在view的页面函数中随意调用该函数
}

/**
 * @brief 采集参数并绘制参数观察页。
 * @param full true 表示整页重绘，false 表示只刷新数值区。
 * @return 无。
 */
static void menu_user_param_draw(bool full)
{
  uint32_t voltage_mv = 0U;
  uint16_t adc_raw = 0U;

  if (adc_convert(ADC4_IN4, &adc_raw) != ZF_OK) {
    adc_raw = 0U;
  }
  if (power_read_voltage_mv(&voltage_mv) != ZF_OK) {
    voltage_mv = 0U;
  }

  if (full) {
    menu_view_user_param_view_11(voltage_mv, adc_raw, HAL_GetTick() / 1000U);
  } else {
    menu_view_user_param_view_11_refresh(voltage_mv, adc_raw,
                                         HAL_GetTick() / 1000U);
  }
}

/**
 * @brief 参数观察页的按键处理函数。
 * @param action 本次按键动作。
 * @return 无。
 * @note 进入页面时整页绘制一次；BACK 退出返回上级菜单。
 */
static void menu_user_param_view(menu_action_enum action)
{
  if ((action == MENU_ACTION_BACK) || (action == MENU_ACTION_BACK_LONG)) {
    menu_exit_function();
    return;
  }

  param_view_last_tick = HAL_GetTick();
  menu_user_param_draw(true);
  menu_request_refresh();
}

/**
 * @brief 参数观察页的周期刷新回调。
 * @return 无。
 * @note 限频到约 4 Hz，只刷新数值区，避免整屏刷新拖慢菜单。
 */
static void menu_user_param_view_poll(void)
{
  uint32_t now = HAL_GetTick();

  if ((uint32_t)(now - param_view_last_tick) < 250U) {
    return;
  }
  param_view_last_tick = now;
  menu_user_param_draw(false);
  menu_request_refresh();
}
