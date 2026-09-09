#ifndef _zf_driver_adc_h_
#define _zf_driver_adc_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 板级 ADC4 通道（按实际引脚命名）。 */
typedef enum {
  ADC4_IN4 = 0, /**< ADC4_IN4  */
  ADC4_IN5,     /**< ADC4_IN5  */
  ADC4_IN7,     /**< ADC4_IN7  */
  ADC4_IN12,      /**< ADC4_IN12 */
  ADC4_IN13,      /**< ADC4_IN13 */
  ADC_CH_NUM
} adc_channel_enum;

/** @brief ADC 初始化参数。 */
typedef struct {
  uint32_t resolution;    /**< ADC_RESOLUTION_xxx。 */
  uint32_t sampling_time; /**< ADC_SAMPLETIME_xxx。 */
  uint32_t vref_mv;       /**< 参考电压，单位毫伏。 */
} adc_cfg_t;

/**
 * @brief 初始化 ADC4 为单通道轮询模式并执行校准。
 * @param cfg 配置参数；传 NULL 使用 bsp_config.h 默认值。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note ADC4 按单通道轮询方式工作，每次采样前重新配置通道。
 */
zf_status_t adc_init(const adc_cfg_t *cfg);

/**
 * @brief 读取指定通道的原始值。
 * @param channel ADC 通道。
 * @param raw 原始值输出。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t adc_convert(adc_channel_enum channel, uint16_t *raw);

/**
 * @brief 读取指定通道并换算为电压。
 * @param channel ADC 通道。
 * @param voltage_mv 电压输出，单位毫伏。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t adc_convert_voltage(adc_channel_enum channel, uint32_t *voltage_mv);

#ifdef __cplusplus
}
#endif

#endif /* _zf_driver_adc_h_ */
