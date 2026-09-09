#include "zf_common_bsp.h"

#include "zf_common_headfile.h"

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

void bsp_init(void)
{
  /* 板级设备：配置各自的引脚并设置初始状态。 */
  led_init();
  key_init();
  buzzer_init();

  /* 外设驱动：完成时钟、GPIO 复用、外设参数初始化。 */
  (void)uart_init(UART_1, NULL);
  (void)uart_init(UART_2, NULL);
  (void)uart_init(UART_3, NULL);
  (void)i2c_init(I2C_2, NULL);
  (void)i2c_init(I2C_4, NULL);
  (void)spi_init(SPI_1, NULL);
  (void)can_init(CAN_2, NULL);
  (void)adc_init(NULL);

  /* 10 ms 周期中断，用于按键扫描。 */
  (void)pit_init(10U);
}
