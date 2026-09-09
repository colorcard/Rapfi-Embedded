#include "zf_driver_adc.h"

#include "zf_common_bsp_config.h"

/** @brief 单次采样的超时时间，单位毫秒。 */
#define ADC_TIMEOUT_MS 10U

/** @brief ADC 句柄，由本模块统一持有。 */
static ADC_HandleTypeDef s_adc;

/** @brief ADC 通道到 HAL 通道号的映射表。 */
static const uint32_t s_adc_channels[ADC_CH_NUM] = {
  ADC_CHANNEL_4,  /* ADC4_IN4  */
  ADC_CHANNEL_5,  /* ADC4_IN5  */
  ADC_CHANNEL_7,  /* ADC4_IN7  */
  ADC_CHANNEL_12, /* ADC4_IN12 */
  ADC_CHANNEL_13, /* ADC4_IN13 */
};

/** @brief 当前采样时间配置。 */
static uint32_t s_sampling_time = ADC_DEFAULT_SAMPLING;

/** @brief 当前参考电压，单位毫伏。 */
static uint32_t s_vref_mv = ADC_VREF_MV;

/** @brief 当前分辨率对应的有效位数。 */
static uint32_t s_resolution_bits = 12U;

zf_status_t adc_init(const adc_cfg_t *cfg)
{
  HAL_StatusTypeDef hal_status;
  uint32_t resolution = (cfg != NULL) ? cfg->resolution : ADC_DEFAULT_RESOLUTION;

  if ((resolution != ADC_RESOLUTION_12B) && (resolution != ADC_RESOLUTION_10B) &&
      (resolution != ADC_RESOLUTION_8B) && (resolution != ADC_RESOLUTION_6B)) {
    return ZF_INVALID_PARAM;
  }
  if ((cfg != NULL) && ((cfg->sampling_time == 0U) || (cfg->vref_mv == 0U))) {
    return ZF_INVALID_PARAM;
  }

  switch (resolution) {
    case ADC_RESOLUTION_10B:
      s_resolution_bits = 10U;
      break;
    case ADC_RESOLUTION_8B:
      s_resolution_bits = 8U;
      break;
    case ADC_RESOLUTION_6B:
      s_resolution_bits = 6U;
      break;
    case ADC_RESOLUTION_12B:
    default:
      s_resolution_bits = 12U;
      break;
  }
  s_sampling_time = (cfg != NULL) ? cfg->sampling_time : ADC_DEFAULT_SAMPLING;
  s_vref_mv = (cfg != NULL) ? cfg->vref_mv : ADC_VREF_MV;

  s_adc.Instance = ADC4;
  s_adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  s_adc.Init.Resolution = resolution;
  s_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  s_adc.Init.GainCompensation = 0U;
  s_adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
  s_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  s_adc.Init.LowPowerAutoWait = DISABLE;
  s_adc.Init.ContinuousConvMode = DISABLE;
  s_adc.Init.NbrOfConversion = 1U;
  s_adc.Init.DiscontinuousConvMode = DISABLE;
  s_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  s_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  s_adc.Init.DMAContinuousRequests = DISABLE;
  s_adc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  s_adc.Init.OversamplingMode = DISABLE;
  hal_status = HAL_ADC_Init(&s_adc);
  if (hal_status != HAL_OK) {
    return zf_from_hal(hal_status);
  }

  return zf_from_hal(HAL_ADCEx_Calibration_Start(&s_adc, ADC_SINGLE_ENDED));
}

zf_status_t adc_convert(adc_channel_enum channel, uint16_t *raw)
{
  ADC_ChannelConfTypeDef config = {0};
  HAL_StatusTypeDef hal_status;

  if ((raw == NULL) || ((uint32_t)channel >= (uint32_t)ADC_CH_NUM)) {
    return ZF_INVALID_PARAM;
  }

  config.Channel = s_adc_channels[channel];
  config.Rank = ADC_REGULAR_RANK_1;
  config.SamplingTime = s_sampling_time;
  config.SingleDiff = ADC_SINGLE_ENDED;
  config.OffsetNumber = ADC_OFFSET_NONE;
  config.Offset = 0U;
  hal_status = HAL_ADC_ConfigChannel(&s_adc, &config);
  if (hal_status != HAL_OK) {
    return zf_from_hal(hal_status);
  }

  hal_status = HAL_ADC_Start(&s_adc);
  if (hal_status != HAL_OK) {
    return zf_from_hal(hal_status);
  }
  hal_status = HAL_ADC_PollForConversion(&s_adc, ADC_TIMEOUT_MS);
  if (hal_status != HAL_OK) {
    (void)HAL_ADC_Stop(&s_adc);
    return zf_from_hal(hal_status);
  }
  *raw = (uint16_t)HAL_ADC_GetValue(&s_adc);
  (void)HAL_ADC_Stop(&s_adc);
  return ZF_OK;
}

zf_status_t adc_convert_voltage(adc_channel_enum channel, uint32_t *voltage_mv)
{
  uint16_t raw;
  zf_status_t status;
  uint32_t full_scale;

  if (voltage_mv == NULL) {
    return ZF_INVALID_PARAM;
  }
  status = adc_convert(channel, &raw);
  if (status != ZF_OK) {
    return status;
  }

  full_scale = (1UL << s_resolution_bits) - 1UL;
  *voltage_mv = ((uint32_t)raw * s_vref_mv + (full_scale / 2U)) / full_scale;
  return ZF_OK;
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adcHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  if (adcHandle->Instance == ADC4) {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC345;
    PeriphClkInit.Adc345ClockSelection = RCC_ADC345CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      error_handler();
    }
    __HAL_RCC_ADC345_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *adcHandle)
{
  if (adcHandle->Instance == ADC4) {
    __HAL_RCC_ADC345_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_14 | GPIO_PIN_15);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
  }
}
