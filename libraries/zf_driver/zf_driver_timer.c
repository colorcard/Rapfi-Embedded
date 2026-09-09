#include "zf_driver_timer.h"

#include "zf_common_bsp_config.h"

/** @brief PWM 引脚复用配置，由 HAL_TIM_PWM_Init() 回调。 */
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timHandle);

/** @brief 定时器句柄，由本模块统一持有。 */
static TIM_HandleTypeDef s_timer[TIMER_NUM];

/** @brief 各定时器需要配置为 PWM 输出的通道。 */
static const uint32_t s_tim1_channels[] = {TIM_CHANNEL_1, TIM_CHANNEL_2,
                                           TIM_CHANNEL_3, TIM_CHANNEL_4};
static const uint32_t s_tim2_channels[] = {TIM_CHANNEL_1, TIM_CHANNEL_2,
                                           TIM_CHANNEL_3, TIM_CHANNEL_4};
static const uint32_t s_tim3_channels[] = {TIM_CHANNEL_1, TIM_CHANNEL_2,
                                           TIM_CHANNEL_3, TIM_CHANNEL_4};
static const uint32_t s_tim4_channels[] = {TIM_CHANNEL_3, TIM_CHANNEL_4};

TIM_HandleTypeDef *timer_get_handle(timer_index_enum index)
{
  if ((uint32_t)index >= (uint32_t)TIMER_NUM) {
    return NULL;
  }
  return &s_timer[index];
}

uint32_t timer_clock_hz(TIM_TypeDef *instance)
{
  uint32_t pclk;
  uint32_t ppre;

  if ((instance == TIM1) || (instance == TIM15) ||
      (instance == TIM16) || (instance == TIM17)) {
    pclk = HAL_RCC_GetPCLK2Freq();
    ppre = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
  } else {
    pclk = HAL_RCC_GetPCLK1Freq();
    ppre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
  }
  return (ppre == 0U) ? pclk : (pclk * 2U);
}

/**
 * @brief 为定时器批量配置 PWM 通道。
 * @param htim 定时器句柄。
 * @param channels 通道数组。
 * @param count 通道数量。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
static zf_status_t timer_pwm_channels_init(TIM_HandleTypeDef *htim,
                                           const uint32_t *channels,
                                           uint32_t count)
{
  TIM_OC_InitTypeDef config = {0};
  uint32_t i;

  config.OCMode = TIM_OCMODE_PWM1;
  config.Pulse = 0U;
  config.OCPolarity = TIM_OCPOLARITY_HIGH;
  config.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  config.OCFastMode = TIM_OCFAST_DISABLE;
  config.OCIdleState = TIM_OCIDLESTATE_RESET;
  config.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  for (i = 0U; i < count; ++i) {
    if (HAL_TIM_PWM_ConfigChannel(htim, &config, channels[i]) != HAL_OK) {
      return ZF_ERROR;
    }
  }
  return ZF_OK;
}

zf_status_t timer_hw_init(timer_index_enum index)
{
  TIM_HandleTypeDef *htim;
  TIM_ClockConfigTypeDef clock_config = {0};
  TIM_MasterConfigTypeDef master_config = {0};

  if ((uint32_t)index >= (uint32_t)TIMER_NUM) {
    return ZF_INVALID_PARAM;
  }
  htim = &s_timer[index];

  htim->Init.CounterMode = TIM_COUNTERMODE_UP;
  htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim->Init.RepetitionCounter = 0U;
  htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  switch (index) {
    case TIMER_1:
      htim->Instance = TIM1;
      htim->Init.Prescaler = 16U;
      htim->Init.Period = 9999U;
      if (HAL_TIM_Base_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      clock_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
      if (HAL_TIM_ConfigClockSource(htim, &clock_config) != HAL_OK) {
        return ZF_ERROR;
      }
      if (HAL_TIM_PWM_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      master_config.MasterOutputTrigger = TIM_TRGO_RESET;
      master_config.MasterOutputTrigger2 = TIM_TRGO2_RESET;
      master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
      if (HAL_TIMEx_MasterConfigSynchronization(htim, &master_config) != HAL_OK) {
        return ZF_ERROR;
      }
      if (timer_pwm_channels_init(htim, s_tim1_channels, 4U) != ZF_OK) {
        return ZF_ERROR;
      }
      {
        TIM_BreakDeadTimeConfigTypeDef break_config = {0};
        break_config.OffStateRunMode = TIM_OSSR_DISABLE;
        break_config.OffStateIDLEMode = TIM_OSSI_DISABLE;
        break_config.LockLevel = TIM_LOCKLEVEL_OFF;
        break_config.BreakState = TIM_BREAK_DISABLE;
        break_config.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
        break_config.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
        break_config.Break2State = TIM_BREAK2_DISABLE;
        break_config.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
        break_config.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
        break_config.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
        if (HAL_TIMEx_ConfigBreakDeadTime(htim, &break_config) != HAL_OK) {
          return ZF_ERROR;
        }
      }
      HAL_TIM_MspPostInit(htim);
      break;

    case TIMER_2:
      htim->Instance = TIM2;
      htim->Init.Prescaler = 16U;
      htim->Init.Period = 9999U;
      if (HAL_TIM_Base_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      clock_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
      if (HAL_TIM_ConfigClockSource(htim, &clock_config) != HAL_OK) {
        return ZF_ERROR;
      }
      if (HAL_TIM_PWM_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      master_config.MasterOutputTrigger = TIM_TRGO_RESET;
      master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
      if (HAL_TIMEx_MasterConfigSynchronization(htim, &master_config) != HAL_OK) {
        return ZF_ERROR;
      }
      if (timer_pwm_channels_init(htim, s_tim2_channels, 4U) != ZF_OK) {
        return ZF_ERROR;
      }
      HAL_TIM_MspPostInit(htim);
      break;

    case TIMER_3:
      htim->Instance = TIM3;
      htim->Init.Prescaler = 16U;
      htim->Init.Period = 9999U;
      if (HAL_TIM_Base_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      clock_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
      if (HAL_TIM_ConfigClockSource(htim, &clock_config) != HAL_OK) {
        return ZF_ERROR;
      }
      if (HAL_TIM_PWM_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      master_config.MasterOutputTrigger = TIM_TRGO_RESET;
      master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
      if (HAL_TIMEx_MasterConfigSynchronization(htim, &master_config) != HAL_OK) {
        return ZF_ERROR;
      }
      if (timer_pwm_channels_init(htim, s_tim3_channels, 4U) != ZF_OK) {
        return ZF_ERROR;
      }
      HAL_TIM_MspPostInit(htim);
      break;

    case TIMER_4:
      htim->Instance = TIM4;
      htim->Init.Prescaler = 16U;
      htim->Init.Period = 9999U;
      if (HAL_TIM_Base_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      clock_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
      if (HAL_TIM_ConfigClockSource(htim, &clock_config) != HAL_OK) {
        return ZF_ERROR;
      }
      if (HAL_TIM_PWM_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      master_config.MasterOutputTrigger = TIM_TRGO_RESET;
      master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
      if (HAL_TIMEx_MasterConfigSynchronization(htim, &master_config) != HAL_OK) {
        return ZF_ERROR;
      }
      if (timer_pwm_channels_init(htim, s_tim4_channels, 2U) != ZF_OK) {
        return ZF_ERROR;
      }
      HAL_TIM_MspPostInit(htim);
      break;

    case TIMER_17:
    default:
      htim->Instance = TIM17;
      htim->Init.Prescaler = 169U;
      htim->Init.Period = 9999U;
      if (HAL_TIM_Base_Init(htim) != HAL_OK) {
        return ZF_ERROR;
      }
      break;
  }
  return ZF_OK;
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *tim_baseHandle)
{
  if (tim_baseHandle->Instance == TIM1) {
    __HAL_RCC_TIM1_CLK_ENABLE();
  } else if (tim_baseHandle->Instance == TIM2) {
    __HAL_RCC_TIM2_CLK_ENABLE();
  } else if (tim_baseHandle->Instance == TIM3) {
    __HAL_RCC_TIM3_CLK_ENABLE();
  } else if (tim_baseHandle->Instance == TIM4) {
    __HAL_RCC_TIM4_CLK_ENABLE();
  } else if (tim_baseHandle->Instance == TIM17) {
    __HAL_RCC_TIM17_CLK_ENABLE();
    HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM17_IRQn, PIT_IRQ_PREEMPT_PRIORITY,
                         PIT_IRQ_SUB_PRIORITY);
    HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM17_IRQn);
  }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *tim_baseHandle)
{
  if (tim_baseHandle->Instance == TIM1) {
    __HAL_RCC_TIM1_CLK_DISABLE();
  } else if (tim_baseHandle->Instance == TIM2) {
    __HAL_RCC_TIM2_CLK_DISABLE();
  } else if (tim_baseHandle->Instance == TIM3) {
    __HAL_RCC_TIM3_CLK_DISABLE();
  } else if (tim_baseHandle->Instance == TIM4) {
    __HAL_RCC_TIM4_CLK_DISABLE();
  } else if (tim_baseHandle->Instance == TIM17) {
    __HAL_RCC_TIM17_CLK_DISABLE();
  }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  if (timHandle->Instance == TIM1) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  } else if (timHandle->Instance == TIM2) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM2;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  } else if (timHandle->Instance == TIM3) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  } else if (timHandle->Instance == TIM4) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  }
}
