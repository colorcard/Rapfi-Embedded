#ifndef _zf_device_usb_cdc_h_
#define _zf_device_usb_cdc_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Type-C 原生 USB（USB FS Device + CDC）。
 * @return ZF_OK 表示成功。
 */
zf_status_t usb_cdc_init(void);

/**
 * @brief USB 低优先级中断处理，转由 PCD 处理。
 * @return 无。
 */
void usb_cdc_irq_handler(void);

/**
 * @brief 主循环任务：把收到的视频帧按条带写入 LCD。
 * @return 无。
 */
void usb_cdc_task(void);

/**
 * @brief 查询是否正在接收视频帧流（用于让出 LCD 给视频）。
 * @return true 表示近期收到过完整帧。
 */
bool usb_cdc_video_active(void);

/**
 * @brief 读取普通 CDC 数据（非视频帧协议）。
 * @param data 输出缓冲。
 * @param max_length 最多读取字节数。
 * @return 实际读取字节数。
 */
uint32_t usb_cdc_read(uint8_t *data, uint32_t max_length);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_usb_cdc_h_ */
