#include "zf_driver_uart.h"

#include <string.h>

#include "zf_common_bsp_config.h"

/** @brief UART 句柄，由本模块统一持有。 */
static UART_HandleTypeDef s_uart[UART_NUM];

/**
 * @brief 获取 UART 外设实例。
 * @param port UART 逻辑编号。
 * @return 外设实例指针；编号非法时返回 NULL。
 */
static USART_TypeDef *uart_instance(uart_index_enum port)
{
  switch (port) {
    case UART_1:
      return USART1;
    case UART_2:
      return USART2;
    case UART_3:
      return USART3;
    default:
      return NULL;
  }
}

/**
 * @brief 获取 UART 句柄。
 * @param port UART 逻辑编号。
 * @return 句柄指针；编号非法时返回 NULL。
 */
static UART_HandleTypeDef *uart_handle(uart_index_enum port)
{
  if ((uint32_t)port >= (uint32_t)UART_NUM) {
    return NULL;
  }
  return &s_uart[port];
}

zf_status_t uart_init(uart_index_enum port, const uart_cfg_t *cfg)
{
  UART_HandleTypeDef *handle;
  USART_TypeDef *instance;

  if ((uint32_t)port >= (uint32_t)UART_NUM) {
    return ZF_INVALID_PARAM;
  }
  handle = &s_uart[port];
  instance = uart_instance(port);
  if (instance == NULL) {
    return ZF_INVALID_PARAM;
  }

  handle->Instance = instance;
  handle->Init.BaudRate = (cfg != NULL) ? cfg->baudrate : UART_DEFAULT_BAUDRATE;
  handle->Init.WordLength = (cfg != NULL) ? cfg->word_length : UART_WORDLENGTH_8B;
  handle->Init.StopBits = (cfg != NULL) ? cfg->stop_bits : UART_STOPBITS_1;
  handle->Init.Parity = (cfg != NULL) ? cfg->parity : UART_PARITY_NONE;
  handle->Init.Mode = UART_MODE_TX_RX;
  handle->Init.HwFlowCtl = UART_HWCONTROL_NONE;
  handle->Init.OverSampling = UART_OVERSAMPLING_16;
  handle->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  handle->Init.ClockPrescaler = UART_PRESCALER_DIV1;
  handle->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(handle) != HAL_OK) {
    return ZF_ERROR;
  }

  (void)HAL_UARTEx_SetTxFifoThreshold(handle, UART_TXFIFO_THRESHOLD_1_8);
  (void)HAL_UARTEx_SetRxFifoThreshold(handle, UART_RXFIFO_THRESHOLD_1_8);
  (void)HAL_UARTEx_DisableFifoMode(handle);
  return ZF_OK;
}

zf_status_t uart_write_buffer(uart_index_enum port, const uint8_t *data,
                              uint16_t length, uint32_t timeout_ms)
{
  UART_HandleTypeDef *handle = uart_handle(port);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return ZF_INVALID_PARAM;
  }
  if (HAL_UART_Transmit(handle, (uint8_t *)data, length, timeout_ms) != HAL_OK) {
    return ZF_ERROR;
  }
  return ZF_OK;
}

zf_status_t uart_write_string(uart_index_enum port, const char *string,
                              uint32_t timeout_ms)
{
  if (string == NULL) {
    return ZF_INVALID_PARAM;
  }
  return uart_write_buffer(port, (const uint8_t *)string,
                           (uint16_t)strlen(string), timeout_ms);
}

zf_status_t uart_read_buffer(uart_index_enum port, uint8_t *data,
                             uint16_t length, uint32_t timeout_ms)
{
  UART_HandleTypeDef *handle = uart_handle(port);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return ZF_INVALID_PARAM;
  }
  if (HAL_UART_Receive(handle, data, length, timeout_ms) != HAL_OK) {
    return ZF_ERROR;
  }
  return ZF_OK;
}

zf_status_t uart_read_byte(uart_index_enum port, uint8_t *byte,
                           uint32_t timeout_ms)
{
  return uart_read_buffer(port, byte, 1U, timeout_ms);
}

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  if (uartHandle->Instance == USART1) {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      error_handler();
    }
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  } else if (uartHandle->Instance == USART2) {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      error_handler();
    }
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  } else if (uartHandle->Instance == USART3) {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART3;
    PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      error_handler();
    }
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle)
{
  if (uartHandle->Instance == USART1) {
    __HAL_RCC_USART1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_4 | GPIO_PIN_5);
  } else if (uartHandle->Instance == USART2) {
    __HAL_RCC_USART2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
  } else if (uartHandle->Instance == USART3) {
    __HAL_RCC_USART3_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
  }
}
