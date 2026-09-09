#include "zf_device_power.h"

#include "zf_common_bsp_config.h"
#include "zf_driver_adc.h"

zf_status_t power_read_voltage_mv(uint32_t *voltage_mv)
{
  uint32_t pin_mv;
  zf_status_t status;

  if (voltage_mv == NULL) {
    return ZF_INVALID_PARAM;
  }

  /* 先读取运放输出端的电压，再按 5.1 倍还原被测电源电压。 */
  status = adc_convert_voltage(ADC4_IN4, &pin_mv);
  if (status != ZF_OK) {
    return status;
  }
  *voltage_mv = (uint32_t)(((uint64_t)pin_mv * POWER_VOLTAGE_SCALE_NUM +
                            (POWER_VOLTAGE_SCALE_DEN / 2U)) /
                           POWER_VOLTAGE_SCALE_DEN);
  return ZF_OK;
}
