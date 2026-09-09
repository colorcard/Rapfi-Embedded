#ifndef _zf_driver_uart_h_
#define _zf_driver_uart_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 板级 UART 逻辑编号。 */
typedef enum {
  UART_1 = 0, /**< USART1：PC4 TX / PC5 RX */
  UART_2,     /**< USART2：PA2 TX / PA3 RX */
  UART_3,     /**< USART3：PB10 TX / PB11 RX */
  UART_NUM
} uart_index_enum;

/** @brief UART 初始化参数。 */
typedef struct {
  uint32_t baudrate;    /**< 波特率，单位 bps。 */
  uint32_t word_length; /**< HAL_UART_WORD_LENGTH_xxx。 */
  uint32_t stop_bits;   /**< UART_STOPBITS_xxx。 */
  uint32_t parity;      /**< UART_PARITY_xxx。 */
} uart_cfg_t;

/**
 * @brief 初始化并配置 UART。
 * @param port UART 逻辑编号。
 * @param cfg 配置参数；传 NULL 使用 bsp_config.h 中的默认值（115200-8N1）。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t uart_init(uart_index_enum port, const uart_cfg_t *cfg);

/**
 * @brief 阻塞发送一段数据。
 * @param port UART 逻辑编号。
 * @param data 待发送缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t uart_write_buffer(uart_index_enum port, const uint8_t *data,
                           uint16_t length, uint32_t timeout_ms);

/**
 * @brief 阻塞发送以 '\0' 结尾的字符串。
 * @param port UART 逻辑编号。
 * @param string 待发送字符串。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t uart_write_string(uart_index_enum port, const char *string,
                                 uint32_t timeout_ms);

/**
 * @brief 阻塞接收指定字节数。
 * @param port UART 逻辑编号。
 * @param data 接收缓冲区。
 * @param length 期望字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t uart_read_buffer(uart_index_enum port, uint8_t *data,
                              uint16_t length, uint32_t timeout_ms);

/**
 * @brief 阻塞接收单个字节。
 * @param port UART 逻辑编号。
 * @param byte 接收字节输出。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t uart_read_byte(uart_index_enum port, uint8_t *byte,
                                  uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* _zf_driver_uart_h_ */
