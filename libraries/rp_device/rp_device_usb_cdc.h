#ifndef _rp_device_usb_cdc_h_
#define _rp_device_usb_cdc_h_

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Type-C 原生 USB（USB FS Device + CDC）。
 * @return RP_OK 表示成功。
 */
rp_status_t usb_cdc_init(void);

/**
 * @brief USB 低优先级中断处理，转由 PCD 处理。
 * @return 无。
 */
void usb_cdc_irq_handler(void);

/**
 * @brief 接收环形缓冲中待读取字节数。
 * @return 字节数。
 */
uint32_t usb_cdc_rx_available(void);

/**
 * @brief 从接收环形缓冲读取一批原始字节（非阻塞，二进制帧用）。
 * @param data 输出缓冲。
 * @param max_length 最多读取字节数。
 * @return 实际读取字节数。
 */
uint32_t usb_cdc_read(uint8_t *data, uint32_t max_length);

/**
 * @brief 通过 CDC IN 端点发送一段原始字节（同步等待完成）。
 * @param data 待发送数据。
 * @param length 字节数。
 * @return RP_OK 表示已发出。
 */
rp_status_t usb_cdc_write(const uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* _rp_device_usb_cdc_h_ */
