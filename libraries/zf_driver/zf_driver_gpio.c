#include "zf_driver_gpio.h"

/**
 * @brief 使能 GPIO 端口对应的 RCC 时钟。
 * @param port GPIO 端口。
 * @return 无。
 * @note 使用端口基址匹配，覆盖 GPIOA~GPIOG，未匹配时不操作。
 */
static void gpio_clock_enable(GPIO_TypeDef *port)
{
  switch ((uintptr_t)port) {
    case GPIOA_BASE:
      __HAL_RCC_GPIOA_CLK_ENABLE();
      break;
    case GPIOB_BASE:
      __HAL_RCC_GPIOB_CLK_ENABLE();
      break;
    case GPIOC_BASE:
      __HAL_RCC_GPIOC_CLK_ENABLE();
      break;
    case GPIOD_BASE:
      __HAL_RCC_GPIOD_CLK_ENABLE();
      break;
    case GPIOE_BASE:
      __HAL_RCC_GPIOE_CLK_ENABLE();
      break;
    case GPIOF_BASE:
      __HAL_RCC_GPIOF_CLK_ENABLE();
      break;
    case GPIOG_BASE:
      __HAL_RCC_GPIOG_CLK_ENABLE();
      break;
    default:
      break;
  }
}

void gpio_init(const gpio_cfg_t *cfg)
{
  GPIO_InitTypeDef init = {0};

  if ((cfg == NULL) || (cfg->port == NULL) || (cfg->pin == 0U)) {
    return;
  }

  gpio_clock_enable(cfg->port);

  init.Pin = cfg->pin;
  init.Mode = cfg->mode;
  init.Pull = cfg->pull;
  init.Speed = cfg->speed;
  init.Alternate = cfg->alternate;
  HAL_GPIO_Init(cfg->port, &init);
}

void gpio_set_level(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
  if ((port == NULL) || (pin == 0U)) {
    return;
  }
  HAL_GPIO_WritePin(port, pin, state);
}

GPIO_PinState gpio_get_level(GPIO_TypeDef *port, uint16_t pin)
{
  if ((port == NULL) || (pin == 0U)) {
    return GPIO_PIN_RESET;
  }
  return HAL_GPIO_ReadPin(port, pin);
}

void gpio_toggle_level(GPIO_TypeDef *port, uint16_t pin)
{
  if ((port == NULL) || (pin == 0U)) {
    return;
  }
  HAL_GPIO_TogglePin(port, pin);
}
