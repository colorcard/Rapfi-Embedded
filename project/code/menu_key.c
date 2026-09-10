#include "menu_key.h"

#include "zf_device_key.h"

#define MENU_KEY_DEBOUNCE_TICKS 1U   /**< 按下消抖所需的 10 ms 周期数。 */
#define MENU_KEY_LONG_TICKS     100U /**< 判定长按所需的 10 ms 周期数。 */

typedef enum {
  MENU_KEY_RELEASED = 0, /**< 按键稳定释放，等待新的按下沿。 */
  MENU_KEY_DEBOUNCE,     /**< 刚检测到按下，正在进行消抖确认。 */
  MENU_KEY_PRESSED,      /**< 已确认按下，正在累计按住时间。 */
  MENU_KEY_LONG_PRESS    /**< 已达到长按阈值，等待释放后上报。 */
} MenuKeyState;

typedef struct {
  MenuKeyState state;           /**< 当前按键状态机阶段。 */
  uint16_t counter;             /**< 消抖或长按持续的 10 ms 计数。 */
  volatile uint8_t short_press; /**< 中断置位、主循环消费的短按标志。 */
  volatile uint8_t long_press;  /**< 中断置位、主循环消费的长按标志。 */
} MenuKeyInfo;

/** @brief 四个菜单动作按顺序对应的 BSP 按键。 */
static const key_index_enum menu_key_map[MENU_KEY_COUNT] = {
  KEY_3, /* Up   - PA6 */
  KEY_2, /* Down - PA5 */
  KEY_4, /* OK   - PA7 */
  KEY_1  /* Back - PA4 */
};

/** @brief 每个物理按键对应的基础短按动作。 */
static const menu_action_enum short_actions[MENU_KEY_COUNT] = {
  MENU_ACTION_UP, MENU_ACTION_DOWN, MENU_ACTION_OK, MENU_ACTION_BACK
};

/** @brief 四个按键各自独立的消抖、计时和事件状态。 */
static MenuKeyInfo menu_keys[MENU_KEY_COUNT];

/**
 * @brief 初始化四个菜单按键的软件状态机。
 * @return 无。
 * @note 应在启动 10 ms 扫描定时器之前调用；引脚初始化由 BSP/GPIO 层负责。
 */
void menu_key_init(void)
{
  /* index 是遍历四个独立按键状态的数组下标。 */
  uint8_t index;
  for (index = 0U; index < MENU_KEY_COUNT; ++index) {
    menu_keys[index].state = MENU_KEY_RELEASED;
    menu_keys[index].counter = 0U;
    menu_keys[index].short_press = 0U;
    menu_keys[index].long_press = 0U;
  }
}

/**
 * @brief 采样四个 BSP 按键并推进消抖及长按状态机。
 * @return 无。
 * @note 本函数设计为每 10 ms 在 TIM17 周期回调中调用一次，有效电平由 BSP 层处理。
 */
void menu_key_scan(void)
{
  /* index 是本次中断扫描所处理的按键数组下标。 */
  uint8_t index;
  for (index = 0U; index < MENU_KEY_COUNT; ++index) {
    /* pressed 将 BSP 层按键状态归一化为布尔按下状态。 */
    uint8_t pressed = key_is_pressed(menu_key_map[index]) ? 1U : 0U;
    /* key 指向本轮正在推进的单个按键状态。 */
    MenuKeyInfo *key = &menu_keys[index];

    switch (key->state) {
      case MENU_KEY_RELEASED:
        if (pressed != 0U) {
          key->state = MENU_KEY_DEBOUNCE;
          key->counter = 0U;
        }
        break;

      case MENU_KEY_DEBOUNCE:
        if (pressed == 0U) {
          key->state = MENU_KEY_RELEASED;
        } else if (++key->counter >= MENU_KEY_DEBOUNCE_TICKS) {
          key->state = MENU_KEY_PRESSED;
          key->counter = 0U;
        }
        break;

      case MENU_KEY_PRESSED:
        if (pressed == 0U) {
          key->short_press = 1U;
          key->state = MENU_KEY_RELEASED;
        } else if (++key->counter >= MENU_KEY_LONG_TICKS) {
          key->state = MENU_KEY_LONG_PRESS;
        }
        break;

      case MENU_KEY_LONG_PRESS:
        if (pressed == 0U) {
          key->long_press = 1U;
          key->state = MENU_KEY_RELEASED;
        }
        break;

      default:
        key->state = MENU_KEY_RELEASED;
        break;
    }
  }
}

/**
 * @brief 读取并消费一个已经完成的按键事件。
 * @return 优先返回长按事件，其次返回短按事件；没有事件时返回
 * MENU_ACTION_WAITING。
 * @note 事件标志由中断写入、主循环读取；每个事件只返回一次。
 */
menu_action_enum menu_key_get_action(void)
{
  /* index 用于按固定优先级依次查找待消费事件。 */
  uint8_t index;
  for (index = 0U; index < MENU_KEY_COUNT; ++index) {
    if (menu_keys[index].long_press != 0U) {
      menu_keys[index].long_press = 0U;
      return (menu_action_enum)(short_actions[index] + 5);
    }
  }
  for (index = 0U; index < MENU_KEY_COUNT; ++index) {
    if (menu_keys[index].short_press != 0U) {
      menu_keys[index].short_press = 0U;
      return short_actions[index];
    }
  }
  return MENU_ACTION_WAITING;
}
