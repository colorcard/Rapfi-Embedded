#ifndef _zf_device_imu660ra_h_
#define _zf_device_imu660ra_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 IMU660RA 总线。
 * @return 当前恒返回 ZF_NOT_READY。
 * @note TODO: 原理图未确认 IMU660RA 使用 SPI 还是 I2C 以及片选/中断引脚，
 *       确认后在此实现总线初始化；数据通道复用 spi_* 或 i2c_*。
 */
zf_status_t imu660ra_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_imu660ra_h_ */
