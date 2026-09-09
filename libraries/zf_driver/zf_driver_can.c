#include "zf_driver_can.h"

#include "zf_common_bsp_config.h"

/** @brief CAN 句柄，由本模块统一持有。 */
static FDCAN_HandleTypeDef s_can[CAN_NUM];

/**
 * @brief 获取 CAN 外设实例。
 * @param bus CAN 逻辑编号。
 * @return 外设实例指针；编号非法时返回 NULL。
 */
static FDCAN_GlobalTypeDef *can_instance(can_index_enum bus)
{
  switch (bus) {
    case CAN_2:
      return FDCAN2;
    default:
      return NULL;
  }
}

/**
 * @brief 获取 CAN 句柄。
 * @param bus CAN 逻辑编号。
 * @return 句柄指针；编号非法时返回 NULL。
 */
static FDCAN_HandleTypeDef *can_handle(can_index_enum bus)
{
  if ((uint32_t)bus >= (uint32_t)CAN_NUM) {
    return NULL;
  }
  return &s_can[bus];
}

/**
 * @brief 根据内核时钟和目标速率计算 FDCAN 位时序。
 * @param clock_hz FDCAN 内核时钟，单位 Hz。
 * @param bitrate 目标速率，单位 bps。
 * @param sample_permille 采样点千分比。
 * @param out_prescaler 输出预分频。
 * @param out_seg1 输出相位段 1。
 * @param out_seg2 输出相位段 2。
 * @return ZF_OK 表示找到合法时序，其他值表示失败。
 */
static zf_status_t can_compute_timing(uint32_t clock_hz, uint32_t bitrate,
                                      uint32_t sample_permille,
                                      uint32_t *out_prescaler,
                                      uint32_t *out_seg1,
                                      uint32_t *out_seg2)
{
  uint32_t best_error = 0xFFFFFFFFU;
  uint32_t best_prescaler = 0U;
  uint32_t best_seg1 = 0U;
  uint32_t best_seg2 = 0U;
  uint32_t prescaler;

  if ((clock_hz == 0U) || (bitrate == 0U) ||
      (sample_permille == 0U) || (sample_permille >= 1000U)) {
    return ZF_INVALID_PARAM;
  }

  for (prescaler = 1U; prescaler <= 512U; ++prescaler) {
    uint64_t denominator = (uint64_t)prescaler * bitrate;
    uint32_t tq_total = (uint32_t)(((uint64_t)clock_hz + (denominator / 2U))
                                   / denominator);
    uint32_t seg2;
    uint32_t seg1;
    uint64_t actual_bitrate;
    uint64_t difference;
    uint32_t error;

    if ((tq_total < 8U) || (tq_total > 385U)) {
      continue;
    }

    seg2 = (uint32_t)(((uint64_t)tq_total * (1000U - sample_permille) + 500U) / 1000U);
    if (seg2 < 1U) {
      seg2 = 1U;
    }
    if (seg2 > 128U) {
      seg2 = 128U;
    }
    if (tq_total <= (1U + seg2)) {
      continue;
    }
    seg1 = tq_total - 1U - seg2;
    if ((seg1 < 1U) || (seg1 > 256U)) {
      continue;
    }

    actual_bitrate = ((uint64_t)clock_hz + ((uint64_t)prescaler * tq_total) / 2U)
                     / ((uint64_t)prescaler * tq_total);
    difference = (actual_bitrate > bitrate) ? (actual_bitrate - bitrate)
                                            : (bitrate - actual_bitrate);
    error = (uint32_t)((difference * 1000000ULL) / bitrate);

    if (error < best_error) {
      best_error = error;
      best_prescaler = prescaler;
      best_seg1 = seg1;
      best_seg2 = seg2;
    }
  }

  if (best_error == 0xFFFFFFFFU) {
    return ZF_ERROR;
  }
  *out_prescaler = best_prescaler;
  *out_seg1 = best_seg1;
  *out_seg2 = best_seg2;
  return ZF_OK;
}

/**
 * @brief 把字节数转换为 FDCAN 的 DLC 编码。
 * @param length 数据字节数。
 * @return 对应的 FDCAN_DLC_BYTES_x；非法长度返回 0xFFFFFFFF。
 */
static uint32_t can_length_to_dlc(uint8_t length)
{
  static const uint8_t table[16] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
                                    12U, 16U, 20U, 24U, 32U, 48U, 64U};
  uint32_t index;

  for (index = 0U; index < 16U; ++index) {
    if (table[index] == length) {
      return index;
    }
  }
  return 0xFFFFFFFFU;
}

/**
 * @brief 把 FDCAN 的 DLC 编码转换为字节数。
 * @param dlc FDCAN_DLC_BYTES_x。
 * @return 数据字节数。
 */
static uint8_t can_dlc_to_length(uint32_t dlc)
{
  static const uint8_t table[16] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
                                    12U, 16U, 20U, 24U, 32U, 48U, 64U};

  return (dlc < 16U) ? table[dlc] : 0U;
}

zf_status_t can_init(can_index_enum bus, const can_cfg_t *cfg)
{
  FDCAN_HandleTypeDef *handle;
  FDCAN_GlobalTypeDef *instance;
  uint32_t frame_format;
  uint32_t nominal_bitrate;
  uint32_t data_bitrate;
  uint32_t sample_permille;
  uint32_t prescaler;
  uint32_t seg1;
  uint32_t seg2;
  FDCAN_FilterTypeDef filter = {0};

  if ((uint32_t)bus >= (uint32_t)CAN_NUM) {
    return ZF_INVALID_PARAM;
  }
  handle = &s_can[bus];
  instance = can_instance(bus);
  if (instance == NULL) {
    return ZF_INVALID_PARAM;
  }

  frame_format = (cfg != NULL) ? cfg->frame_format : FDCAN_FRAME_CLASSIC;
  nominal_bitrate = (cfg != NULL) ? cfg->nominal_bitrate : CAN_DEFAULT_BITRATE;
  data_bitrate = (cfg != NULL) ? cfg->data_bitrate : CAN_DEFAULT_BITRATE;
  sample_permille = (cfg != NULL) ? cfg->sample_point_permille
                                  : CAN_DEFAULT_SAMPLE_PERMILLE;

  if (can_compute_timing(HAL_RCC_GetPCLK1Freq(), nominal_bitrate,
                         sample_permille, &prescaler, &seg1, &seg2) != ZF_OK) {
    return ZF_INVALID_PARAM;
  }

  handle->Instance = instance;
  handle->Init.ClockDivider = FDCAN_CLOCK_DIV1;
  handle->Init.FrameFormat = frame_format;
  handle->Init.Mode = FDCAN_MODE_NORMAL;
  handle->Init.AutoRetransmission = ENABLE;
  handle->Init.TransmitPause = ENABLE;
  handle->Init.ProtocolException = DISABLE;
  handle->Init.NominalPrescaler = prescaler;
  handle->Init.NominalSyncJumpWidth = (seg2 > 16U) ? 16U : seg2;
  handle->Init.NominalTimeSeg1 = seg1;
  handle->Init.NominalTimeSeg2 = seg2;
  handle->Init.StdFiltersNbr = 1U;
  handle->Init.ExtFiltersNbr = 1U;
  handle->Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

  if (frame_format != FDCAN_FRAME_CLASSIC) {
    if (can_compute_timing(HAL_RCC_GetPCLK1Freq(), data_bitrate,
                           sample_permille, &prescaler, &seg1, &seg2) != ZF_OK) {
      return ZF_INVALID_PARAM;
    }
    handle->Init.DataPrescaler = prescaler;
    handle->Init.DataSyncJumpWidth = (seg2 > 16U) ? 16U : seg2;
    handle->Init.DataTimeSeg1 = seg1;
    handle->Init.DataTimeSeg2 = seg2;
  }

  if (HAL_FDCAN_Init(handle) != HAL_OK) {
    return ZF_ERROR;
  }

  /* 标准帧全通滤波器。 */
  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0U;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = 0x00000000U;
  filter.FilterID2 = 0x00000000U;
  if (HAL_FDCAN_ConfigFilter(handle, &filter) != HAL_OK) {
    return ZF_ERROR;
  }

  /* 扩展帧全通滤波器。 */
  filter.IdType = FDCAN_EXTENDED_ID;
  filter.FilterIndex = 0U;
  if (HAL_FDCAN_ConfigFilter(handle, &filter) != HAL_OK) {
    return ZF_ERROR;
  }

  if (HAL_FDCAN_ConfigGlobalFilter(handle, FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_FILTER_REMOTE,
                                   FDCAN_FILTER_REMOTE) != HAL_OK) {
    return ZF_ERROR;
  }

  return can_start(bus);
}

zf_status_t can_start(can_index_enum bus)
{
  FDCAN_HandleTypeDef *handle = can_handle(bus);

  if (handle == NULL) {
    return ZF_INVALID_PARAM;
  }
  if (HAL_FDCAN_Start(handle) != HAL_OK) {
    return ZF_ERROR;
  }
  return ZF_OK;
}

zf_status_t can_stop(can_index_enum bus)
{
  FDCAN_HandleTypeDef *handle = can_handle(bus);

  if (handle == NULL) {
    return ZF_INVALID_PARAM;
  }
  if (HAL_FDCAN_Stop(handle) != HAL_OK) {
    return ZF_ERROR;
  }
  return ZF_OK;
}

zf_status_t can_send(can_index_enum bus, const can_message_t *message,
                     uint32_t timeout_ms)
{
  FDCAN_HandleTypeDef *handle = can_handle(bus);
  FDCAN_TxHeaderTypeDef tx_header = {0};
  uint32_t dlc;
  uint32_t free_before;
  uint32_t start_tick;

  if ((handle == NULL) || (message == NULL)) {
    return ZF_INVALID_PARAM;
  }
  dlc = can_length_to_dlc(message->length);
  if (dlc == 0xFFFFFFFFU) {
    return ZF_INVALID_PARAM;
  }
  if ((message->fd_format == false) && (message->length > 8U)) {
    return ZF_INVALID_PARAM;
  }

  tx_header.Identifier = message->id;
  tx_header.IdType = message->extended ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  tx_header.TxFrameType = FDCAN_DATA_FRAME;
  tx_header.DataLength = dlc;
  tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  tx_header.BitRateSwitch = message->fd_format ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
  tx_header.FDFormat = message->fd_format ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
  tx_header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  tx_header.MessageMarker = 0U;

  /* 记录入队前的空闲槽位，本帧发送完成后空闲槽位会恢复到该值。 */
  free_before = HAL_FDCAN_GetTxFifoFreeLevel(handle);
  if (free_before == 0U) {
    return ZF_ERROR;
  }
  if (HAL_FDCAN_AddMessageToTxFifoQ(handle, &tx_header,
                                    (uint8_t *)message->data) != HAL_OK) {
    return ZF_ERROR;
  }

  start_tick = HAL_GetTick();
  while (HAL_FDCAN_GetTxFifoFreeLevel(handle) < free_before) {
    if ((HAL_GetTick() - start_tick) >= timeout_ms) {
      return ZF_TIMEOUT;
    }
  }
  return ZF_OK;
}

zf_status_t can_receive(can_index_enum bus, can_message_t *message,
                        uint32_t timeout_ms)
{
  FDCAN_HandleTypeDef *handle = can_handle(bus);
  FDCAN_RxHeaderTypeDef rx_header = {0};
  uint32_t start_tick;

  if ((handle == NULL) || (message == NULL)) {
    return ZF_INVALID_PARAM;
  }

  start_tick = HAL_GetTick();
  while (HAL_FDCAN_GetRxFifoFillLevel(handle, FDCAN_RX_FIFO0) == 0U) {
    if ((HAL_GetTick() - start_tick) >= timeout_ms) {
      return ZF_TIMEOUT;
    }
  }

  if (HAL_FDCAN_GetRxMessage(handle, FDCAN_RX_FIFO0, &rx_header,
                             message->data) != HAL_OK) {
    return ZF_ERROR;
  }

  message->id = rx_header.Identifier;
  message->extended = (rx_header.IdType == FDCAN_EXTENDED_ID) ? true : false;
  message->fd_format = (rx_header.FDFormat == FDCAN_FD_CAN) ? true : false;
  message->length = can_dlc_to_length(rx_header.DataLength);
  return ZF_OK;
}

bool can_is_message_pending(can_index_enum bus)
{
  FDCAN_HandleTypeDef *handle = can_handle(bus);

  if (handle == NULL) {
    return false;
  }
  return (HAL_FDCAN_GetRxFifoFillLevel(handle, FDCAN_RX_FIFO0) > 0U) ? true : false;
}

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *fdcanHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  if (fdcanHandle->Instance == FDCAN2) {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      error_handler();
    }
    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  }
}

void HAL_FDCAN_MspDeInit(FDCAN_HandleTypeDef *fdcanHandle)
{
  if (fdcanHandle->Instance == FDCAN2) {
    __HAL_RCC_FDCAN_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_12 | GPIO_PIN_13);
  }
}
