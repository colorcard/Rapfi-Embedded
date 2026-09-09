#ifndef _zf_device_nrf24l01_h_
#define _zf_device_nrf24l01_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------- NRF24L01 寄存器地址 ---------------------- */
#define NRF24L01_CONFIG      0x00U /**< 配置寄存器。 */
#define NRF24L01_EN_AA       0x01U /**< 自动应答使能。 */
#define NRF24L01_EN_RXADDR   0x02U /**< 接收管道使能。 */
#define NRF24L01_SETUP_AW    0x03U /**< 地址宽度设置。 */
#define NRF24L01_SETUP_RETR  0x04U /**< 自动重发设置。 */
#define NRF24L01_RF_CH       0x05U /**< 射频通道。 */
#define NRF24L01_RF_SETUP    0x06U /**< 射频速率与发射功率。 */
#define NRF24L01_STATUS      0x07U /**< 状态寄存器。 */
#define NRF24L01_OBSERVE_TX  0x08U /**< 发送观测寄存器。 */
#define NRF24L01_RPD         0x09U /**< 接收功率检测。 */
#define NRF24L01_RX_ADDR_P0  0x0AU /**< 接收管道 0 地址。 */
#define NRF24L01_RX_ADDR_P1  0x0BU /**< 接收管道 1 地址。 */
#define NRF24L01_RX_ADDR_P2  0x0CU /**< 接收管道 2 地址。 */
#define NRF24L01_RX_ADDR_P3  0x0DU /**< 接收管道 3 地址。 */
#define NRF24L01_RX_ADDR_P4  0x0EU /**< 接收管道 4 地址。 */
#define NRF24L01_RX_ADDR_P5  0x0FU /**< 接收管道 5 地址。 */
#define NRF24L01_TX_ADDR     0x10U /**< 发送地址。 */
#define NRF24L01_RX_PW_P0    0x11U /**< 接收管道 0 载荷宽度。 */
#define NRF24L01_RX_PW_P1    0x12U /**< 接收管道 1 载荷宽度。 */
#define NRF24L01_RX_PW_P2    0x13U /**< 接收管道 2 载荷宽度。 */
#define NRF24L01_RX_PW_P3    0x14U /**< 接收管道 3 载荷宽度。 */
#define NRF24L01_RX_PW_P4    0x15U /**< 接收管道 4 载荷宽度。 */
#define NRF24L01_RX_PW_P5    0x16U /**< 接收管道 5 载荷宽度。 */
#define NRF24L01_FIFO_STATUS 0x17U /**< FIFO 状态寄存器。 */
#define NRF24L01_DYNPD       0x1CU /**< 动态载荷长度使能。 */
#define NRF24L01_FEATURE     0x1DU /**< 特性寄存器。 */

/* ------------------------ NRF24L01 命令字 ------------------------ */
#define NRF24L01_R_REGISTER          0x00U /**< 读寄存器。 */
#define NRF24L01_W_REGISTER          0x20U /**< 写寄存器。 */
#define NRF24L01_R_RX_PAYLOAD        0x61U /**< 读接收载荷。 */
#define NRF24L01_W_TX_PAYLOAD        0xA0U /**< 写发送载荷。 */
#define NRF24L01_FLUSH_TX            0xE1U /**< 清空发送 FIFO。 */
#define NRF24L01_FLUSH_RX            0xE2U /**< 清空接收 FIFO。 */
#define NRF24L01_REUSE_TX_PL         0xE3U /**< 重用上次发送载荷。 */
#define NRF24L01_R_RX_PL_WID         0x60U /**< 读接收载荷宽度。 */
#define NRF24L01_W_ACK_PAYLOAD       0xA8U /**< 写带应答载荷（低 3 位为管道号）。 */
#define NRF24L01_W_TX_PAYLOAD_NO_ACK 0xB0U /**< 写发送载荷且不要求应答。 */
#define NRF24L01_NOP                 0xFFU /**< 空操作，用于读状态。 */

/**
 * @brief 初始化 NRF24L01 接口（PB1=CSN、PB2=CE，SPI1 预分频 32）。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note 仅初始化引脚与 SPI 总线，不配置 NRF24L01 内部寄存器。
 */
zf_status_t nrf24l01_init(void);

/**
 * @brief 写单个寄存器。
 * @param reg 寄存器地址。
 * @param value 待写入的值。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t nrf24l01_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief 读单个寄存器。
 * @param reg 寄存器地址。
 * @param value 读取结果输出。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t nrf24l01_read_reg(uint8_t reg, uint8_t *value);

/**
 * @brief 连续写寄存器（地址自增）。
 * @param reg 起始寄存器地址。
 * @param data 待写入数据。
 * @param length 字节数。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t nrf24l01_write_buffer(uint8_t reg, const uint8_t *data,
                                  uint8_t length);

/**
 * @brief 连续读寄存器（地址自增）。
 * @param reg 起始寄存器地址。
 * @param data 读取结果输出。
 * @param length 字节数。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t nrf24l01_read_buffer(uint8_t reg, uint8_t *data, uint8_t length);

/**
 * @brief 写入发送载荷（W_TX_PAYLOAD）。
 * @param data 待发送数据。
 * @param length 字节数，1~32。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t nrf24l01_write_tx_payload(const uint8_t *data, uint8_t length);

/**
 * @brief 读取接收载荷（R_RX_PAYLOAD）。
 * @param data 读取结果输出。
 * @param length 字节数，1~32。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t nrf24l01_read_rx_payload(uint8_t *data, uint8_t length);

/**
 * @brief 清空发送 FIFO（FLUSH_TX）。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t nrf24l01_flush_tx(void);

/**
 * @brief 清空接收 FIFO（FLUSH_RX）。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t nrf24l01_flush_rx(void);

/**
 * @brief 设置 CE 引脚电平。
 * @param level 非 0 拉高（使能收发），0 拉低（待机）。
 * @return 无。
 */
void nrf24l01_ce(uint8_t level);

/**
 * @brief 设置 CSN 引脚电平。
 * @param level 非 0 拉高（取消片选），0 拉低（选中）。
 * @return 无。
 */
void nrf24l01_csn(uint8_t level);

#ifdef __cplusplus
}
#endif

#endif /* _zf_device_nrf24l01_h_ */
