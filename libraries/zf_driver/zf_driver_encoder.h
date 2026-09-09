#ifndef _zf_driver_encoder_h_
#define _zf_driver_encoder_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 编码器逻辑编号。 */
typedef enum {
  ENCODER_1 = 0,
  ENCODER_2,
  ENCODER_NUM
} encoder_index_enum;

/**
 * @brief 初始化编码器接口。
 * @param encoder 编码器编号。
 * @return 当前恒返回 ZF_NOT_READY。
 * @note TODO: 原理图未确认编码器与定时器通道的对应关系，确认后在此实现
 *       编码器模式初始化（引脚复用与定时器通道共用）。
 */
zf_status_t encoder_init(encoder_index_enum encoder);

/**
 * @brief 读取编码器计数。
 * @param encoder 编码器编号。
 * @return 当前恒返回 0。
 */
int32_t encoder_get_count(encoder_index_enum encoder);

/**
 * @brief 清零编码器计数。
 * @param encoder 编码器编号。
 * @return 无。
 */
void encoder_reset(encoder_index_enum encoder);

#ifdef __cplusplus
}
#endif

#endif /* _zf_driver_encoder_h_ */
