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
 * @brief 从接收环形缓冲取一批字节（非阻塞）。
 * @param data 输出缓冲。
 * @param max_length 最多读取字节数。
 * @return 实际读取字节数。
 */
uint32_t usb_cdc_read(uint8_t *data, uint32_t max_length);

/**
 * @brief 从接收环形缓冲取一个字节（非阻塞）。
 * @param out 输出字节。
 * @return 1 取到，0 无数据。
 */
int usb_cdc_try_read_byte(uint8_t *out);

/**
 * @brief 通过 CDC IN 端点发送一段数据（忙时等待至超时）。
 * @param data 待发送数据。
 * @param length 字节数。
 * @return RP_OK 表示已发出。
 */
rp_status_t usb_cdc_write(const uint8_t *data, uint32_t length);

/**
 * @brief 发送以 '\0' 结尾的字符串。
 * @param s 字符串。
 * @return RP_OK 表示已发出。
 */
rp_status_t usb_cdc_write_str(const char *s);

/**
 * @brief 格式化并发送（内部单缓冲，勿在中断中调用）。
 * @param fmt printf 风格格式串。
 * @return RP_OK 表示已发出。
 */
rp_status_t usb_cdc_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _rp_device_usb_cdc_h_ */
