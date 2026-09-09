#ifndef _zf_device_nrf24l01_h_
#define _zf_device_nrf24l01_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 NRF24L01 接口。
 * @return 当前恒返回 ZF_NOT_READY。
 * @note TODO: 原理图未确认 CSN/CE/IRQ 的 GPIO，确认后在此实现片选、使能引脚
 *       初始化和中断配置；SPI 数据通道可直接使用 spi_* 接口。
 */
zf_status_t nrf24l01_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_nrf24l01_h_ */
