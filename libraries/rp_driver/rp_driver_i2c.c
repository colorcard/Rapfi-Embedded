#include "rp_driver_i2c.h"

#include "rp_common_bsp_config.h"

/** @brief SCLL/SCLH 字段最大计数（寄存器值 + 1）。 */
#define I2C_COUNT_MAX 256U

/** @brief I2C 句柄，由本模块统一持有。 */
static I2C_HandleTypeDef s_i2c[I2C_NUM];

/**
 * @brief 获取 I2C 外设实例。
 * @param bus I2C 逻辑编号。
 * @return 外设实例指针；编号非法时返回 NULL。
 */
static I2C_TypeDef *i2c_instance(i2c_index_enum bus)
{
  switch (bus) {
    case I2C_2:
      return I2C2;
    case I2C_4:
      return I2C4;
    default:
      return NULL;
  }
}

/**
 * @brief 获取 I2C 句柄。
 * @param bus I2C 逻辑编号。
 * @return 句柄指针；编号非法时返回 NULL。
 */
static I2C_HandleTypeDef *i2c_handle(i2c_index_enum bus)
{
  if ((uint32_t)bus >= (uint32_t)I2C_NUM) {
    return NULL;
  }
  return &s_i2c[bus];
}

/**
 * @brief 按 I2C 规范计算 TIMINGR 寄存器值。
 * @param clock_hz I2C 内核时钟频率，单位 Hz。
 * @param speed_hz 目标总线速率，单位 Hz。
 * @return 计算得到的 TIMINGR 值；参数非法时返回 0。
 * @note 采用 RM0440 时序公式：t_PRESC=(PRESC+1)/f_I2CCLK，
 *       t_SCLL=(SCLL+1)*t_PRESC，t_SCLH=(SCLH+1)*t_PRESC，
 *       总线周期 = t_SCLL + t_SCLH + 上升时间 + 下降时间。
 */
static uint32_t i2c_compute_timing(uint32_t clock_hz, uint32_t speed_hz)
{
  uint32_t low_min_ns;
  uint32_t high_min_ns;
  uint32_t sudat_ns;
  uint32_t period_ns;
  uint32_t driven_ns;
  uint32_t best_timing = 0U;
  uint32_t best_error = 0xFFFFFFFFU;
  uint32_t presc;

  if ((clock_hz == 0U) || (speed_hz == 0U)) {
    return 0U;
  }

  if (speed_hz > 100000U) {
    low_min_ns = 1300U;  /* Fast mode t_LOW(min) */
    high_min_ns = 600U;  /* Fast mode t_HIGH(min) */
    sudat_ns = 100U;     /* Fast mode t_SU;DAT */
  } else {
    low_min_ns = 4700U;  /* Standard mode t_LOW(min) */
    high_min_ns = 4000U; /* Standard mode t_HIGH(min) */
    sudat_ns = 250U;     /* Standard mode t_SU;DAT */
  }

  period_ns = 1000000000U / speed_hz;
  if (period_ns <= (I2C_RISE_TIME_NS + I2C_FALL_TIME_NS)) {
    return 0U;
  }
  driven_ns = period_ns - I2C_RISE_TIME_NS - I2C_FALL_TIME_NS;

  for (presc = 0U; presc <= 15U; ++presc) {
    uint64_t tpresc_over_clk = ((uint64_t)(presc + 1U) * 1000000000ULL);
    uint32_t total_counts;
    uint32_t low_counts;
    uint32_t high_counts;
    uint32_t high_min_counts;
    uint32_t actual_ns;
    uint32_t error;

    total_counts = (uint32_t)(((uint64_t)driven_ns * clock_hz) / tpresc_over_clk);
    if ((total_counts < 4U) || (total_counts > (2U * I2C_COUNT_MAX))) {
      continue;
    }

    low_counts = (uint32_t)((((uint64_t)low_min_ns * clock_hz) + tpresc_over_clk - 1ULL)
                            / tpresc_over_clk);
    high_min_counts = (uint32_t)((((uint64_t)high_min_ns * clock_hz) + tpresc_over_clk - 1ULL)
                                 / tpresc_over_clk);
    if (low_counts > I2C_COUNT_MAX) {
      continue;
    }
    high_counts = total_counts - low_counts;
    if (high_counts < high_min_counts) {
      if (high_min_counts > total_counts) {
        continue;
      }
      high_counts = high_min_counts;
      low_counts = total_counts - high_counts;
      if (low_counts > I2C_COUNT_MAX) {
        continue;
      }
    }
    if ((high_counts > I2C_COUNT_MAX) || (low_counts == 0U) || (high_counts == 0U)) {
      continue;
    }

    actual_ns = (uint32_t)(((uint64_t)(total_counts) * tpresc_over_clk) / clock_hz)
                + I2C_RISE_TIME_NS + I2C_FALL_TIME_NS;
    error = (actual_ns > period_ns) ? (actual_ns - period_ns) : (period_ns - actual_ns);

    if (error < best_error) {
      uint32_t scldel = (uint32_t)((((uint64_t)sudat_ns * clock_hz) + tpresc_over_clk - 1ULL)
                                   / tpresc_over_clk);
      uint32_t sdadel = (uint32_t)((((uint64_t)I2C_FALL_TIME_NS * clock_hz)
                                    + tpresc_over_clk - 1ULL) / tpresc_over_clk);
      if (scldel > 0U) {
        scldel -= 1U;
      }
      if (sdadel > 0U) {
        sdadel -= 1U;
      }
      if ((scldel > 15U) || (sdadel > 15U)) {
        /* 该预分频无法满足 t_SU;DAT / t_HD;DAT 最小值，换下一个预分频。 */
        continue;
      }
      best_error = error;
      best_timing = ((uint32_t)presc << 28U)
                    | (scldel << 20U)
                    | (sdadel << 16U)
                    | ((high_counts - 1U) << 8U)
                    | (low_counts - 1U);
    }
  }

  return best_timing;
}

rp_status_t i2c_init(i2c_index_enum bus, const i2c_cfg_t *cfg)
{
  I2C_HandleTypeDef *handle;
  I2C_TypeDef *instance;
  uint32_t speed_hz;
  uint32_t timing;
  HAL_StatusTypeDef hal_status;

  if ((uint32_t)bus >= (uint32_t)I2C_NUM) {
    return RP_INVALID_PARAM;
  }
  handle = &s_i2c[bus];
  instance = i2c_instance(bus);
  if (instance == NULL) {
    return RP_INVALID_PARAM;
  }

  speed_hz = (cfg != NULL) ? cfg->clock_speed_hz : I2C_DEFAULT_SPEED_HZ;
  timing = i2c_compute_timing(HAL_RCC_GetPCLK1Freq(), speed_hz);
  if (timing == 0U) {
    return RP_INVALID_PARAM;
  }

  handle->Instance = instance;
  handle->Init.Timing = timing;
  handle->Init.OwnAddress1 = ((cfg != NULL) && (cfg->own_address != 0U))
                             ? (uint16_t)(cfg->own_address << 1U) : 0U;
  handle->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  handle->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  handle->Init.OwnAddress2 = 0U;
  handle->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  handle->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  handle->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  hal_status = HAL_I2C_Init(handle);
  if (hal_status != HAL_OK) {
    return rp_from_hal(hal_status);
  }

  (void)HAL_I2CEx_ConfigAnalogFilter(handle, I2C_ANALOGFILTER_ENABLE);
  (void)HAL_I2CEx_ConfigDigitalFilter(handle, 2U);
  return RP_OK;
}

rp_status_t i2c_write(i2c_index_enum bus, uint8_t address,
                      const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
  I2C_HandleTypeDef *handle = i2c_handle(bus);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }
  return rp_from_hal(HAL_I2C_Master_Transmit(handle, (uint16_t)(address << 1U),
                                             (uint8_t *)data, length,
                                             timeout_ms));
}

rp_status_t i2c_read(i2c_index_enum bus, uint8_t address,
                     uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
  I2C_HandleTypeDef *handle = i2c_handle(bus);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }
  return rp_from_hal(HAL_I2C_Master_Receive(handle, (uint16_t)(address << 1U),
                                            data, length, timeout_ms));
}

rp_status_t i2c_mem_write(i2c_index_enum bus, uint8_t address,
                          uint16_t mem_address, uint16_t mem_address_size,
                          const uint8_t *data, uint16_t length,
                          uint32_t timeout_ms)
{
  I2C_HandleTypeDef *handle = i2c_handle(bus);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }
  return rp_from_hal(HAL_I2C_Mem_Write(handle, (uint16_t)(address << 1U),
                                       mem_address, mem_address_size,
                                       (uint8_t *)data, length, timeout_ms));
}

rp_status_t i2c_mem_read(i2c_index_enum bus, uint8_t address,
                         uint16_t mem_address, uint16_t mem_address_size,
                         uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
  I2C_HandleTypeDef *handle = i2c_handle(bus);

  if ((handle == NULL) || (data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }
  return rp_from_hal(HAL_I2C_Mem_Read(handle, (uint16_t)(address << 1U),
                                      mem_address, mem_address_size, data,
                                      length, timeout_ms));
}

rp_status_t i2c_is_device_ready(i2c_index_enum bus, uint8_t address,
                                uint32_t trials, uint32_t timeout_ms)
{
  I2C_HandleTypeDef *handle = i2c_handle(bus);

  if (handle == NULL) {
    return RP_INVALID_PARAM;
  }
  return rp_from_hal(HAL_I2C_IsDeviceReady(handle, (uint16_t)(address << 1U),
                                           trials, timeout_ms));
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *i2cHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  if (i2cHandle->Instance == I2C2) {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
    PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      error_handler();
    }
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    __HAL_RCC_I2C2_CLK_ENABLE();
  } else if (i2cHandle->Instance == I2C4) {
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
    PeriphClkInit.I2c4ClockSelection = RCC_I2C4CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
      error_handler();
    }
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF8_I2C4;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    __HAL_RCC_I2C4_CLK_ENABLE();
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *i2cHandle)
{
  if (i2cHandle->Instance == I2C2) {
    __HAL_RCC_I2C2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_8 | GPIO_PIN_9);
  } else if (i2cHandle->Instance == I2C4) {
    __HAL_RCC_I2C4_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6 | GPIO_PIN_7);
  }
}
