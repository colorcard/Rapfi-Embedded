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

/** @brief 调试用计数：收到完整帧数 / 写入条带数 / 收到字节数。 */
extern volatile uint32_t g_video_frame_cnt;
extern volatile uint32_t g_video_strip_cnt;
extern volatile uint32_t g_video_rx_bytes;
/** @brief 刷屏耗时（DWT 周期）：最近一次 / 历史最大。 */
extern volatile uint32_t g_video_present_cycles;
extern volatile uint32_t g_video_present_max_cycles;
/** @brief 其中用于展开像素的周期数（最近一次）。 */
extern volatile uint32_t g_video_expand_cycles;

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_usb_cdc_h_ */
