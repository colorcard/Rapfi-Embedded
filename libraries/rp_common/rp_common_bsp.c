#include "rp_common_bsp.h"

#include "rp_common_headfile.h"

/**
 * @brief 全局 MSP 初始化，由 HAL_Init() 调用。
 * @return 无。
 */
void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* 关闭 UCPD Dead Battery 引脚的内部上拉。 */
  HAL_PWREx_DisableUCPDDeadBattery();
}

/**
 * @brief 致命错误处理：关闭中断并停在此处。
 * @return 无。
 */
void error_handler(void)
{
  __disable_irq();
  while (1) {
  }
}

/**
 * @brief 记录驱动初始化结果，保留第一个错误。
 * @param status 当前累计状态。
 * @param result 本次初始化结果。
 * @return 更新后的累计状态。
 */
static rp_status_t bsp_collect(rp_status_t status, rp_status_t result)
{
  if ((result != RP_OK) && (status == RP_OK)) {
    return result;
  }
  return status;
}

rp_status_t bsp_init(void)
{
  rp_status_t status = RP_OK;

  /* 板级设备：只初始化 rp_common_bsp_config.h 中打开的模块。 */
#if (BSP_ENABLE_KEY != 0U)
  key_init();
#endif

  /* 外设驱动：完成时钟、GPIO 复用与参数初始化。 */
#if (BSP_ENABLE_UART1 != 0U)
  status = bsp_collect(status, uart_init(UART_1, NULL));
#endif
#if (BSP_ENABLE_UART2 != 0U)
  status = bsp_collect(status, uart_init(UART_2, NULL));
#endif
#if (BSP_ENABLE_UART3 != 0U)
  status = bsp_collect(status, uart_init(UART_3, NULL));
#endif
#if (BSP_ENABLE_SPI1 != 0U)
  status = bsp_collect(status, spi_init(SPI_1, NULL));
#endif

  /* 周期中断：周期任务。 */
#if (BSP_ENABLE_PIT != 0U)
  status = bsp_collect(status, pit_init(PIT_DEFAULT_PERIOD_MS));
#endif

  /* 独立看门狗：初始化后需在主循环周期性调用 iwdg_feed()。 */
#if (BSP_ENABLE_IWDG != 0U)
  status = bsp_collect(status, iwdg_init(IWDG_DEFAULT_TIMEOUT_MS));
#endif

  return status;
}
