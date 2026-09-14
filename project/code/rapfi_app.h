#ifndef _RAPFI_APP_H_
#define _RAPFI_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 应用初始化：引擎初始化、清空棋盘、打印就绪信息。
 * @return 无。
 * @note 需在 usb_cdc_init() 之后调用。
 */
void app_init(void);

/**
 * @brief 应用轮询：处理 USB 命令（后续接入按键/LCD）。
 * @return 无。
 */
void app_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* _RAPFI_APP_H_ */
