#include "rp_driver_uart.h"

#include <string.h>

#include "rp_common_bsp_config.h"
#include "rp_common_fifo.h"

/** @brief UART 句柄，由本模块统一持有。 */
static UART_HandleTypeDef s_uart[UART_NUM];

/** @brief 每路 UART 的接收环形缓冲。 */
static rp_fifo_t s_rx_fifo[UART_NUM];
static uint8_t s_rx_storage[UART_NUM][UART_RX_BUFFER_SIZE];

/** @brief 接收中断单字节暂存。 */
static uint8_t s_rx_byte[UART_NUM];

/** @brief 接收中断回调与透传参数。 */
static uart_rx_callback_t s_rx_callback[UART_NUM];
static void *s_rx_context[UART_NUM];

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
 * @brief 由 HAL 句柄反查逻辑编号。
 * @param handle HAL 句柄。
 * @param port 逻辑编号输出。
 * @return true 表示找到对应编号。
 */
static bool uart_port_of(const UART_HandleTypeDef *handle, uint32_t *port)
{
  uint32_t index;

  if ((handle == NULL) || (port == NULL)) {
    return false;
  }
  for (index = 0U; index < (uint32_t)UART_NUM; ++index) {
    if (handle->Instance == uart_instance((uart_index_enum)index)) {
      *port = index;
      return true;
    }
  }
  return false;
}

UART_HandleTypeDef *uart_get_handle(uart_index_enum port)
{
  if ((uint32_t)port >= (uint32_t)UART_NUM) {
    return NULL;
  }
  return &s_uart[port];
}

rp_status_t uart_init(uart_index_enum port, const uart_cfg_t *cfg)
{
  UART_HandleTypeDef *handle;
  USART_TypeDef *instance;
  HAL_StatusTypeDef hal_status;

  if ((uint32_t)port >= (uint32_t)UART_NUM) {
    return RP_INVALID_PARAM;
  }
  handle = &s_uart[port];
  instance = uart_instance(port);
  if (instance == NULL) {
    return RP_INVALID_PARAM;
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
  hal_status = HAL_UART_Init(handle);
  if (hal_status != HAL_OK) {
    return rp_from_hal(hal_status);
  }

  fifo_init(&s_rx_fifo[port], s_rx_storage[port], UART_RX_BUFFER_SIZE);
  return rp_from_hal(HAL_UART_Receive_IT(handle, &s_rx_byte[port], 1U));
}

rp_status_t uart_write_buffer(uart_index_enum port, const uint8_t *data,
                              uint16_t length, uint32_t timeout_ms)
{
  UART_HandleTypeDef *handle = uart_get_handle(port);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }
  return rp_from_hal(HAL_UART_Transmit(handle, (uint8_t *)data, length,
                                       timeout_ms));
}

rp_status_t uart_write_string(uart_index_enum port, const char *string,
                              uint32_t timeout_ms)
{
  if (string == NULL) {
    return RP_INVALID_PARAM;
  }
  return uart_write_buffer(port, (const uint8_t *)string,
                           (uint16_t)strlen(string), timeout_ms);
}

rp_status_t uart_read_buffer(uart_index_enum port, uint8_t *data,
                             uint16_t length, uint32_t timeout_ms)
{
  uint32_t got = 0U;
  uint32_t start_tick;

  if (((uint32_t)port >= (uint32_t)UART_NUM) || (data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }

  start_tick = HAL_GetTick();
  while (got < length) {
    got += fifo_read_buffer(&s_rx_fifo[port], &data[got], length - got);
    if (got >= length) {
      break;
    }
    if ((HAL_GetTick() - start_tick) >= timeout_ms) {
      return RP_TIMEOUT;
    }
  }
  return RP_OK;
}

rp_status_t uart_read_byte(uart_index_enum port, uint8_t *byte,
                           uint32_t timeout_ms)
{
  uint32_t start_tick;

  if (((uint32_t)port >= (uint32_t)UART_NUM) || (byte == NULL)) {
    return RP_INVALID_PARAM;
  }

  start_tick = HAL_GetTick();
  while (fifo_read_byte(&s_rx_fifo[port], byte) != RP_OK) {
    if ((HAL_GetTick() - start_tick) >= timeout_ms) {
      return RP_TIMEOUT;
    }
  }
  return RP_OK;
}

rp_status_t uart_query_byte(uart_index_enum port, uint8_t *byte)
{
  if (((uint32_t)port >= (uint32_t)UART_NUM) || (byte == NULL)) {
    return RP_INVALID_PARAM;
  }
  return fifo_read_byte(&s_rx_fifo[port], byte);
}

rp_status_t uart_set_callback(uart_index_enum port, uart_rx_callback_t callback,
                              void *context)
{
  if ((uint32_t)port >= (uint32_t)UART_NUM) {
    return RP_INVALID_PARAM;
  }
  s_rx_callback[port] = callback;
  s_rx_context[port] = context;
  return RP_OK;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint32_t port;

  if (uart_port_of(huart, &port)) {
    (void)fifo_write_byte(&s_rx_fifo[port], s_rx_byte[port]);
    if (s_rx_callback[port] != NULL) {
      s_rx_callback[port](s_rx_byte[port], s_rx_context[port]);
    }
    (void)HAL_UART_Receive_IT(huart, &s_rx_byte[port], 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  uint32_t port;

  if (uart_port_of(huart, &port)) {
    /* 清除错误后重新挂起接收，避免一次噪声导致接收永久停止。 */
    (void)HAL_UART_AbortReceive(huart);
    (void)HAL_UART_Receive_IT(huart, &s_rx_byte[port], 1U);
  }
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
    /* 扩展板 H2“排母串口接口”使用 PC4=TX / PC5=RX（PA9 让给 I2C2 IMU）。 */
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(USART1_IRQn, UART_IRQ_PREEMPT_PRIORITY, UART_IRQ_SUB_PRIORITY);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
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
    HAL_NVIC_SetPriority(USART2_IRQn, UART_IRQ_PREEMPT_PRIORITY, UART_IRQ_SUB_PRIORITY);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
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
    HAL_NVIC_SetPriority(USART3_IRQn, UART_IRQ_PREEMPT_PRIORITY, UART_IRQ_SUB_PRIORITY);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle)
{
  if (uartHandle->Instance == USART1) {
    __HAL_RCC_USART1_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_4 | GPIO_PIN_5);
  } else if (uartHandle->Instance == USART2) {
    __HAL_RCC_USART2_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
  } else if (uartHandle->Instance == USART3) {
    __HAL_RCC_USART3_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(USART3_IRQn);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
  }
}
