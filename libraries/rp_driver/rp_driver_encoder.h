#ifndef _rp_driver_encoder_h_
#define _rp_driver_encoder_h_

#include "rp_common_bsp.h"
#include "rp_driver_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 编码器逻辑编号。
 * @note ENCODER_1 对应 TIM2 正交编码器（CH1=PD3、CH2=PD4，连接器 CN2）；
 *       ENCODER_2 对应 TIM3 正交编码器（CH1=PE2、CH2=PE3，连接器 CN3）。
 */
typedef enum {
  ENCODER_1 = 0, /**< TIM2 正交编码器，PD3/PD4（CN2） */
  ENCODER_2,     /**< TIM3 正交编码器，PE2/PE3（CN3） */
  ENCODER_NUM    /**< 编码器编号数量 */
} encoder_index_enum;

/**
 * @brief 初始化编码器接口。
 * @param encoder 编码器编号。
 * @return RP_OK 表示成功，RP_INVALID_PARAM 表示编号非法，其他值表示失败。
 * @note 内部先调用 timer_hw_init() 完成定时器时钟与引脚复用配置，再按
 *       TIM_ENCODERMODE_TI12 正交模式（四倍频）初始化并启动编码器接口。
 */
rp_status_t encoder_init(encoder_index_enum encoder);

/**
 * @brief 读取编码器计数。
 * @param encoder 编码器编号。
 * @return 16 位有符号计数值（硬件计数器按 16 位环绕）；编号非法时返回 0。
 */
int32_t encoder_get_count(encoder_index_enum encoder);

/**
 * @brief 清零编码器计数。
 * @param encoder 编码器编号。
 * @return 无。
 * @note 编号非法时不执行任何操作。
 */
void encoder_reset(encoder_index_enum encoder);

#ifdef __cplusplus
}
#endif

#endif /* _rp_driver_encoder_h_ */
