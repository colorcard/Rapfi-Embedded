#ifndef _zf_common_debug_h_
#define _zf_common_debug_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 调试串口使用的 UART 逻辑编号，可在本头文件按需修改。 */
#define DEBUG_UART_INDEX UART_1

/** @brief debug_printf 静态格式化缓冲区大小，单位字节。 */
#define DEBUG_PRINTF_CAPACITY 160U

/** @brief 调试串口发送超时时间，单位毫秒。 */
#define DEBUG_UART_TIMEOUT_MS 1000U

/**
 * @brief 初始化调试串口。
 * @return 无。
 * @note 内部调用 uart_init(DEBUG_UART_INDEX, NULL)，成功后置位就绪标志；
 *       未初始化或初始化失败时，debug_write/debug_printf 直接返回。
 */
void debug_init(void);

/**
 * @brief 通过调试串口阻塞发送一段数据。
 * @param data 待发送缓冲区指针。
 * @param length 待发送字节数。
 * @return 无。
 * @note 未初始化、data 为 NULL 或 length 为 0 时直接返回。
 */
void debug_write(const uint8_t *data, uint16_t length);

/**
 * @brief 通过调试串口阻塞发送格式化字符串。
 * @param format 格式化字符串，语法与 printf 相同。
 * @param ... 可变参数。
 * @return 无。
 * @note 使用静态缓冲区格式化，最长输出 DEBUG_PRINTF_CAPACITY - 1 个字符；
 *       未初始化或 format 为 NULL 时直接返回。
 */
void debug_printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* _zf_common_debug_h_ */
