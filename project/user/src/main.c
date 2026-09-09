#include "zf_common_headfile.h"
#include "menu_core.h"

/** @brief 最近的电源电压采样值，单位毫伏；供 SWD 在线观测。 */
volatile uint32_t g_power_voltage_mv;
/** @brief 电源电压通道的 ADC 原始值（0~4095），用于判断是否饱和。 */
volatile uint16_t g_power_adc_raw;

/**
 * @brief 应用入口。
 * @return 不会返回。
 * @note 初始化顺序：HAL -> 时钟 -> 板级外设 -> 显示 -> 菜单。
 */
int main(void)
{
  uint32_t last_report;
  uint32_t voltage_mv;
  uint16_t adc_raw;

  HAL_Init();
  clock_init();
  if (bsp_init() != ZF_OK) {
    error_handler();
  }

  debug_init();
  lcd_init(LCD_DIRECTION_LANDSCAPE);
  menu_init(50U);
  last_report = HAL_GetTick();

  while (1) {
    menu_process();

    /* 每秒通过调试串口上报一次电源电压，同时刷新供 SWD 观测的全局变量。 */
    if ((HAL_GetTick() - last_report) >= 1000U) {
      last_report = HAL_GetTick();
      if (adc_convert(ADC4_IN4, &adc_raw) == ZF_OK) {
        g_power_adc_raw = adc_raw;
      }
      if (power_read_voltage_mv(&voltage_mv) == ZF_OK) {
        g_power_voltage_mv = voltage_mv;
        debug_printf("PWR=%lu mV raw=%u\r\n", (unsigned long)voltage_mv,
                     (unsigned int)adc_raw);
      } else {
        debug_printf("PWR=read fail\r\n");
      }
    }

#if (BSP_ENABLE_IWDG != 0U)
    iwdg_feed();
#endif
  }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief 参数断言失败处理。
 * @param file 源文件名。
 * @param line 出错行号。
 * @return 无。
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
  while (1) {
  }
}
#endif
