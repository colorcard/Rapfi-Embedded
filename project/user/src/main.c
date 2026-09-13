#include "zf_common_headfile.h"
#ifdef APP_LVGL_DEMO
#include "zf_device_lcd_lvgl.h"
#else
#include "menu_core.h"
#endif

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

  /* 上电 IMU 自检：等待首帧数据后打印，便于确认 I2C 接线。 */
  {
    int16_t imu_acc[3];
    int16_t imu_gyro[3];

    system_delay_ms(100);
    if ((imu660ra_read_accel(imu_acc) == ZF_OK) &&
        (imu660ra_read_gyro(imu_gyro) == ZF_OK)) {
      debug_printf("IMU A=%d,%d,%d G=%d,%d,%d\r\n", (int)imu_acc[0],
                   (int)imu_acc[1], (int)imu_acc[2], (int)imu_gyro[0],
                   (int)imu_gyro[1], (int)imu_gyro[2]);
    } else {
      debug_printf("IMU read fail\r\n");
    }
  }

#ifdef APP_LVGL_DEMO
  (void)lcd_hw_init();
  lcd_lvgl_init();
  lcd_lvgl_demo();
#else
  lcd_init(LCD_DIRECTION_LANDSCAPE);
  menu_init(50U);
#endif
  last_report = HAL_GetTick();

  while (1) {
#ifdef APP_LVGL_DEMO
    lcd_lvgl_handler();
#else
    menu_process();
#endif

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
