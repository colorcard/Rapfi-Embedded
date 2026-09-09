#ifndef _zf_device_power_h_
#define _zf_device_power_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 读取电源（电池）电压。
 * @param voltage_mv 电压输出，单位毫伏。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 采样通道为 PB14 / ADC4_IN4，经 OPA2188 按 5.1 倍缩放。
 *       结果为估算值，受 VDDA 基准与分压电阻误差影响，可用 POWER_VOLTAGE_SCALE_*
 *       按实测校准。
 */
zf_status_t power_read_voltage_mv(uint32_t *voltage_mv);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_power_h_ */
