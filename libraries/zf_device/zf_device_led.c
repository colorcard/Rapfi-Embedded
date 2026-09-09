#include "zf_device_led.h"

#include "zf_common_bsp_config.h"
#include "zf_driver_gpio.h"

/** @brief LED 的端口与引脚映射。 */
typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
} led_hw_t;

/** @brief 按逻辑编号排列的 LED 硬件映射表。 */
static const led_hw_t s_leds[LED_NUM] = {
  { GPIOA, GPIO_PIN_0 }, /* LED_STATUS */
};

/**
 * @brief 按有效电平写入 LED 引脚。
 * @param led LED 逻辑编号。
 * @param on 非零表示点亮。
 * @return 无。
 */
static void led_write(led_index_enum led, uint8_t on)
{
  GPIO_PinState state;

  if ((uint32_t)led >= (uint32_t)LED_NUM) {
    return;
  }
#if (LED_ACTIVE_HIGH != 0U)
  state = (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
#else
  state = (on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET;
#endif
  gpio_set_level(s_leds[led].port, s_leds[led].pin, state);
}

void led_init(void)
{
  const gpio_cfg_t config = {
    GPIOA, GPIO_PIN_0, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0U
  };

  gpio_init(&config);
  led_write(LED_STATUS, 0U);
}

void led_on(led_index_enum led)
{
  led_write(led, 1U);
}

void led_off(led_index_enum led)
{
  led_write(led, 0U);
}

void led_toggle(led_index_enum led)
{
  if ((uint32_t)led >= (uint32_t)LED_NUM) {
    return;
  }
  gpio_toggle_level(s_leds[led].port, s_leds[led].pin);
}
