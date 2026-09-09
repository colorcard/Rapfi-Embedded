#ifndef _zf_common_bsp_h_
#define _zf_common_bsp_h_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "stm32g4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief BSP 层统一返回状态。 */
typedef enum {
  ZF_OK = 0,            /**< 操作成功。 */
  ZF_ERROR = -1,        /**< 通用错误。 */
  ZF_TIMEOUT = -2,      /**< 操作超时。 */
  ZF_NOT_READY = -3,    /**< 功能尚未就绪（引脚或参数待确认）。 */
  ZF_INVALID_PARAM = -4 /**< 参数非法。 */
} zf_status_t;

/**
 * @brief 把 HAL 返回状态映射为 BSP 状态。
 * @param status HAL 状态。
 * @return HAL_OK -> ZF_OK；HAL_TIMEOUT -> ZF_TIMEOUT；HAL_BUSY/HAL_ERROR -> ZF_ERROR。
 */
static inline zf_status_t zf_from_hal(HAL_StatusTypeDef status)
{
  switch (status) {
    case HAL_OK:      return ZF_OK;
    case HAL_TIMEOUT: return ZF_TIMEOUT;
    case HAL_BUSY:
    case HAL_ERROR:
    default:          return ZF_ERROR;
  }
}

/**
 * @brief 初始化板级软件状态。
 * @return 无。
 * @note 必须在 clock_init() 之后调用。本函数是外设初始化的唯一入口：
 *       依次完成板级设备引脚配置、外设时钟/复用/参数初始化与周期中断启动。
 */
zf_status_t bsp_init(void);

/**
 * @brief 致命错误处理：关闭中断并停在此处。
 * @return 无。
 * @note 供各驱动初始化失败时调用，便于调试器定位。
 */
void error_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* _zf_common_bsp_h_ */
