#ifndef _zf_driver_spi_h_
#define _zf_driver_spi_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 板级 SPI 逻辑编号。 */
typedef enum {
  SPI_1 = 0, /**< SPI1：PB3 SCK / PB4 MISO / PB5 MOSI（NRF24L01、LCD 共用） */
  SPI_2,     /**< SPI2：PB13 SCK / PB14 MISO / PB15 MOSI（PB12 片选，W25Q128） */
  SPI_NUM
} spi_index_enum;

/** @brief SPI 初始化参数。 */
typedef struct {
  uint32_t baud_rate_prescaler; /**< SPI_BAUDRATEPRESCALER_xxx。 */
  uint32_t clk_polarity;        /**< SPI_POLARITY_LOW / SPI_POLARITY_HIGH。 */
  uint32_t clk_phase;           /**< SPI_PHASE_1EDGE / SPI_PHASE_2EDGE。 */
  uint32_t data_size;           /**< SPI_DATASIZE_8BIT / SPI_DATASIZE_16BIT。 */
  uint32_t first_bit;           /**< SPI_FIRSTBIT_MSB / SPI_FIRSTBIT_LSB。 */
} spi_cfg_t;

/**
 * @brief 初始化并配置 SPI。
 * @param bus SPI 逻辑编号。
 * @param cfg 配置参数；传 NULL 使用 bsp_config.h 中的默认值。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t spi_init(spi_index_enum bus, const spi_cfg_t *cfg);

/**
 * @brief 动态修改 SPI 波特率预分频（同一总线上的不同器件可各自设置）。
 * @param bus SPI 逻辑编号。
 * @param prescaler SPI_BAUDRATEPRESCALER_xxx。
 * @return ZF_OK 或 ZF_INVALID_PARAM。
 */
zf_status_t spi_set_prescaler(spi_index_enum bus, uint32_t prescaler);

/**
 * @brief 阻塞发送数据（忽略接收）。
 * @param bus SPI 逻辑编号。
 * @param data 待发送缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t spi_write_8bit_array(spi_index_enum bus, const uint8_t *data,
                              uint16_t length, uint32_t timeout_ms);

/**
 * @brief 阻塞接收数据（发送 0xFF 填充时钟）。
 * @param bus SPI 逻辑编号。
 * @param data 接收缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t spi_read_8bit_array(spi_index_enum bus, uint8_t *data,
                             uint16_t length, uint32_t timeout_ms);

/**
 * @brief 全双工收发。
 * @param bus SPI 逻辑编号。
 * @param tx 发送缓冲区。
 * @param rx 接收缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t spi_transfer_8bit(spi_index_enum bus, const uint8_t *tx, uint8_t *rx,
                              uint16_t length, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* _zf_driver_spi_h_ */
