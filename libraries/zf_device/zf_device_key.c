#include "zf_device_key.h"

#include "zf_common_bsp_config.h"
#include "zf_driver_gpio.h"

/** @brief 按键的端口与引脚映射。 */
typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
} key_hw_t;

/** @brief 按逻辑编号排列的按键硬件映射表。 */
static const key_hw_t s_keys[KEY_NUM] = {
  { GPIOA, GPIO_PIN_4 }, /* KEY_1 */
  { GPIOA, GPIO_PIN_5 }, /* KEY_2 */
  { GPIOA, GPIO_PIN_6 }, /* KEY_3 */
  { GPIOA, GPIO_PIN_7 }, /* KEY_4 */
};

void key_init(void)
{
  const gpio_cfg_t config = {
    GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
    GPIO_MODE_INPUT, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0U
  };

  gpio_init(&config);
}

bool key_is_pressed(key_index_enum key)
{
  GPIO_PinState state;

  if ((uint32_t)key >= (uint32_t)KEY_NUM) {
    return false;
  }
  state = gpio_get_level(s_keys[key].port, s_keys[key].pin);
#if (KEY_ACTIVE_LOW != 0U)
  return (state == GPIO_PIN_RESET) ? true : false;
#else
  return (state == GPIO_PIN_SET) ? true : false;
#endif
}
