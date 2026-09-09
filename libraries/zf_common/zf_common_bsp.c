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

zf_status_t bsp_init(void)
{
  zf_status_t status = ZF_OK;
  zf_status_t result;

  /* 板级设备：配置各自的引脚并设置初始状态。 */
  led_init();
  key_init();
  buzzer_init();

  /* 外设驱动：完成时钟、GPIO 复用、外设参数初始化。 */
  result = uart_init(UART_1, NULL);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }
  result = uart_init(UART_2, NULL);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }
  result = uart_init(UART_3, NULL);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }
  result = i2c_init(I2C_2, NULL);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }
  result = i2c_init(I2C_4, NULL);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }
  result = spi_init(SPI_1, NULL);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }
  result = can_init(CAN_2, NULL);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }
  result = adc_init(NULL);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }

  /* 周期中断，用于按键扫描。 */
  result = pit_init(PIT_DEFAULT_PERIOD_MS);
  if ((result != ZF_OK) && (status == ZF_OK)) {
    status = result;
  }
  return status;
}
