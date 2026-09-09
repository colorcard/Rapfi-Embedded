#include "zf_driver_encoder.h"

/** @brief 编码器逻辑编号到定时器的硬件映射表。 */
static const timer_index_enum s_encoder_timer[ENCODER_NUM] = {
  TIMER_2, /* ENCODER_1：TIM2 CH1/CH2，PD3/PD4（CN2） */
  TIMER_3, /* ENCODER_2：TIM3 CH1/CH2，PE2/PE3（CN3） */
};

zf_status_t encoder_init(encoder_index_enum encoder)
{
  TIM_HandleTypeDef *htim;
  TIM_Encoder_InitTypeDef config = {0};
  HAL_StatusTypeDef hal_status;
  zf_status_t status;

  if ((uint32_t)encoder >= (uint32_t)ENCODER_NUM) {
    return ZF_INVALID_PARAM;
  }
  status = timer_hw_init(s_encoder_timer[encoder]);
  if (status != ZF_OK) {
    return status;
  }
  htim = timer_get_handle(s_encoder_timer[encoder]);
  if (htim == NULL) {
    return ZF_INVALID_PARAM;
  }

  config.EncoderMode = TIM_ENCODERMODE_TI12;
  config.IC1Polarity = TIM_ICPOLARITY_RISING;
  config.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  config.IC1Prescaler = TIM_ICPSC_DIV1;
  config.IC1Filter = 0U;
  config.IC2Polarity = TIM_ICPOLARITY_RISING;
  config.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  config.IC2Prescaler = TIM_ICPSC_DIV1;
  config.IC2Filter = 0U;

  hal_status = HAL_TIM_Encoder_Init(htim, &config);
  if (hal_status != HAL_OK) {
    return zf_from_hal(hal_status);
  }
  return zf_from_hal(HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL));
}

int32_t encoder_get_count(encoder_index_enum encoder)
{
  TIM_HandleTypeDef *htim;

  if ((uint32_t)encoder >= (uint32_t)ENCODER_NUM) {
    return 0;
  }
  htim = timer_get_handle(s_encoder_timer[encoder]);
  if (htim == NULL) {
    return 0;
  }
  return (int32_t)(int16_t)__HAL_TIM_GET_COUNTER(htim);
}

void encoder_reset(encoder_index_enum encoder)
{
  TIM_HandleTypeDef *htim;

  if ((uint32_t)encoder >= (uint32_t)ENCODER_NUM) {
    return;
  }
  htim = timer_get_handle(s_encoder_timer[encoder]);
  if (htim != NULL) {
    __HAL_TIM_SET_COUNTER(htim, 0U);
  }
}
