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

/** @brief 接收中断回调类型。 */
typedef void (*uart_rx_callback_t)(uint8_t data, void *context);

/**
 * @brief 获取 UART HAL 句柄。
 * @param port UART 逻辑编号。
 * @return 句柄指针；编号非法时返回 NULL。
 */
UART_HandleTypeDef *uart_get_handle(uart_index_enum port);

/**
 * @brief 初始化 UART，开启接收中断并把接收数据写入环形缓冲。
 * @param port UART 逻辑编号。
 * @param cfg 配置参数；传 NULL 使用默认值（115200-8N1）。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t uart_init(uart_index_enum port, const uart_cfg_t *cfg);

/**
 * @brief 阻塞发送一段数据。
 * @param port UART 逻辑编号。
 * @param data 待发送缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，ZF_TIMEOUT 表示超时。
 */
zf_status_t uart_write_buffer(uart_index_enum port, const uint8_t *data,
                              uint16_t length, uint32_t timeout_ms);

/**
 * @brief 阻塞发送以 '\0' 结尾的字符串。
 * @param port UART 逻辑编号。
 * @param string 待发送字符串。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，ZF_TIMEOUT 表示超时。
 */
zf_status_t uart_write_string(uart_index_enum port, const char *string,
                              uint32_t timeout_ms);

/**
 * @brief 从接收环形缓冲读取指定字节数（阻塞）。
 * @param port UART 逻辑编号。
 * @param data 接收缓冲区。
 * @param length 期望字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示读满 length 字节；ZF_TIMEOUT 表示超时，已读走的字节不会回退。
 */
zf_status_t uart_read_buffer(uart_index_enum port, uint8_t *data,
                             uint16_t length, uint32_t timeout_ms);

/**
 * @brief 从接收环形缓冲读取单个字节（阻塞）。
 * @param port UART 逻辑编号。
 * @param byte 接收字节输出。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，ZF_TIMEOUT 表示超时。
 */
zf_status_t uart_read_byte(uart_index_enum port, uint8_t *byte,
                           uint32_t timeout_ms);

/**
 * @brief 从接收环形缓冲查询一个字节（非阻塞）。
 * @param port UART 逻辑编号。
 * @param byte 接收字节输出。
 * @return ZF_OK 表示取到数据，ZF_ERROR 表示当前无数据。
 */
zf_status_t uart_query_byte(uart_index_enum port, uint8_t *byte);

/**
 * @brief 注册接收中断回调。
 * @param port UART 逻辑编号。
 * @param callback 回调函数，传 NULL 取消。
 * @param context 回调透传参数。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 回调在中断上下文中执行，应尽量简短。
 */
zf_status_t uart_set_callback(uart_index_enum port, uart_rx_callback_t callback,
                              void *context);

#ifdef __cplusplus
}
#endif

#endif /* _zf_driver_uart_h_ */
