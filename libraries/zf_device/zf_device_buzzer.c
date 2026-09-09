#include "zf_device_buzzer.h"

#include "zf_common_bsp_config.h"

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
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, state);
}

void buzzer_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);

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
