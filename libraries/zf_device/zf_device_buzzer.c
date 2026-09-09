#include "zf_device_buzzer.h"

#include "zf_common_bsp_config.h"
#include "zf_driver_gpio.h"

#define BUZZER_PORT GPIOA
#define BUZZER_PIN  GPIO_PIN_1

/**
 * @brief 按有效电平写入蜂鸣器控制引脚。
 * @param on 非零表示鸣叫。
 * @return 无。
 */
static void buzzer_write(uint8_t on)
{
  GPIO_PinState state;

#if (BUZZER_ACTIVE_HIGH != 0U)
  state = (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
#else
  state = (on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET;
#endif
  gpio_set_level(BUZZER_PORT, BUZZER_PIN, state);
}

void buzzer_init(void)
{
  const gpio_cfg_t config = {
    BUZZER_PORT, BUZZER_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL,
    GPIO_SPEED_FREQ_LOW, 0U
  };

  gpio_init(&config);
  buzzer_write(0U);
}

void buzzer_on(void)
{
  buzzer_write(1U);
}

void buzzer_off(void)
{
  buzzer_write(0U);
}

void buzzer_beep(uint32_t duration_ms)
{
  buzzer_on();
  HAL_Delay(duration_ms);
  buzzer_off();
}
