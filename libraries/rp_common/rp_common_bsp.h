#ifndef _rp_common_bsp_h_
#define _rp_common_bsp_h_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "stm32g4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief BSP 层统一返回状态。 */
typedef enum {
  RP_OK = 0,            /**< 操作成功。 */
  RP_ERROR = -1,        /**< 通用错误。 */
  RP_TIMEOUT = -2,      /**< 操作超时。 */
  RP_NOT_READY = -3,    /**< 功能尚未就绪（引脚或参数待确认）。 */
  RP_INVALID_PARAM = -4 /**< 参数非法。 */
} rp_status_t;

/**
 * @brief 把 HAL 返回状态映射为 BSP 状态。
 * @param status HAL 状态。
 * @return HAL_OK -> RP_OK；HAL_TIMEOUT -> RP_TIMEOUT；HAL_BUSY/HAL_ERROR -> RP_ERROR。
 */
static inline rp_status_t rp_from_hal(HAL_StatusTypeDef status)
{
  switch (status) {
    case HAL_OK:      return RP_OK;
    case HAL_TIMEOUT: return RP_TIMEOUT;
    case HAL_BUSY:
    case HAL_ERROR:
    default:          return RP_ERROR;
  }
}

/**
 * @brief 初始化板级软件状态。
 * @return 无。
 * @note 必须在 clock_init() 之后调用。本函数是外设初始化的唯一入口：
 *       依次完成板级设备引脚配置、外设时钟/复用/参数初始化与周期中断启动。
 */
rp_status_t bsp_init(void);

/**
 * @brief 致命错误处理：关闭中断并停在此处。
 * @return 无。
 * @note 供各驱动初始化失败时调用，便于调试器定位。
 */
void error_handler(void);

#ifdef __cplusplus
}
#endif

#endif /* _rp_common_bsp_h_ */
