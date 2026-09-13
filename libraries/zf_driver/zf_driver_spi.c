#include "zf_driver_spi.h"

#include "zf_common_bsp_config.h"

/** @brief SPI 句柄，由本模块统一持有。 */
static SPI_HandleTypeDef s_spi[SPI_NUM];

/**
 * @brief 获取 SPI 外设实例。
 * @param bus SPI 逻辑编号。
 * @return 外设实例指针；编号非法时返回 NULL。
 */
static SPI_TypeDef *spi_instance(spi_index_enum bus)
{
  switch (bus) {
    case SPI_1:
      return SPI1;
    case SPI_2:
      return SPI2;
    default:
      return NULL;
  }
}

/**
 * @brief 获取 SPI 句柄。
 * @param bus SPI 逻辑编号。
 * @return 句柄指针；编号非法时返回 NULL。
 */
static SPI_HandleTypeDef *spi_handle(spi_index_enum bus)
{
  if ((uint32_t)bus >= (uint32_t)SPI_NUM) {
    return NULL;
  }
  return &s_spi[bus];
}

zf_status_t spi_init(spi_index_enum bus, const spi_cfg_t *cfg)
{
  SPI_HandleTypeDef *handle;
  SPI_TypeDef *instance;

  if ((uint32_t)bus >= (uint32_t)SPI_NUM) {
    return ZF_INVALID_PARAM;
  }
  handle = &s_spi[bus];
  instance = spi_instance(bus);
  if (instance == NULL) {
    return ZF_INVALID_PARAM;
  }

  handle->Instance = instance;
  handle->Init.Mode = SPI_MODE_MASTER;
  handle->Init.Direction = SPI_DIRECTION_2LINES;
  handle->Init.DataSize = (cfg != NULL) ? cfg->data_size : SPI_DATASIZE_8BIT;
  handle->Init.CLKPolarity = (cfg != NULL) ? cfg->clk_polarity : SPI_DEFAULT_CPOL;
  handle->Init.CLKPhase = (cfg != NULL) ? cfg->clk_phase : SPI_DEFAULT_CPHA;
  handle->Init.NSS = SPI_NSS_SOFT;
  handle->Init.BaudRatePrescaler =
      (cfg != NULL) ? cfg->baud_rate_prescaler : SPI_DEFAULT_PRESCALER;
  handle->Init.FirstBit = (cfg != NULL) ? cfg->first_bit : SPI_FIRSTBIT_MSB;
  handle->Init.TIMode = SPI_TIMODE_DISABLE;
  handle->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  handle->Init.CRCPolynomial = 7;
  handle->Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  handle->Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  return zf_from_hal(HAL_SPI_Init(handle));
}

zf_status_t spi_set_prescaler(spi_index_enum bus, uint32_t prescaler)
{
  SPI_HandleTypeDef *handle = spi_handle(bus);

  if ((handle == NULL) || (handle->Instance == NULL) ||
      ((prescaler & ~(uint32_t)SPI_CR1_BR) != 0U)) {
    return ZF_INVALID_PARAM;
  }

  handle->Init.BaudRatePrescaler = prescaler;
  __HAL_SPI_DISABLE(handle);
  MODIFY_REG(handle->Instance->CR1, SPI_CR1_BR, prescaler);
  __HAL_SPI_ENABLE(handle);
  return ZF_OK;
}

zf_status_t spi_write_8bit_array(spi_index_enum bus, const uint8_t *data,
                                 uint16_t length, uint32_t timeout_ms)
{
  SPI_HandleTypeDef *handle = spi_handle(bus);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return ZF_INVALID_PARAM;
  }
  return zf_from_hal(HAL_SPI_Transmit(handle, (uint8_t *)data, length,
                                      timeout_ms));
}

zf_status_t spi_read_8bit_array(spi_index_enum bus, uint8_t *data,
                                uint16_t length, uint32_t timeout_ms)
{
  SPI_HandleTypeDef *handle = spi_handle(bus);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return ZF_INVALID_PARAM;
  }
  return zf_from_hal(HAL_SPI_Receive(handle, data, length, timeout_ms));
}

zf_status_t spi_transfer_8bit(spi_index_enum bus, const uint8_t *tx, uint8_t *rx,
                              uint16_t length, uint32_t timeout_ms)
{
  SPI_HandleTypeDef *handle = spi_handle(bus);

  if ((handle == NULL) || (tx == NULL) || (rx == NULL) || (length == 0U)) {
    return ZF_INVALID_PARAM;
  }
  return zf_from_hal(HAL_SPI_TransmitReceive(handle, (uint8_t *)tx, rx, length,
                                             timeout_ms));
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *spiHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (spiHandle->Instance == SPI1) {
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  } else if (spiHandle->Instance == SPI2) {
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /* SPI2：PB13 SCK / PB14 MISO / PB15 MOSI；PB12 作为片选由设备层控制。 */
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *spiHandle)
{
  if (spiHandle->Instance == SPI1) {
    __HAL_RCC_SPI1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);
  } else if (spiHandle->Instance == SPI2) {
    __HAL_RCC_SPI2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
  }
}
