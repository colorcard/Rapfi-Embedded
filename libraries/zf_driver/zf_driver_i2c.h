#ifndef _zf_driver_i2c_h_
#define _zf_driver_i2c_h_

#include "zf_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 板级 I2C 逻辑编号。 */
typedef enum {
  I2C_2 = 0, /**< I2C2：PA8 SDA / PA9 SCL */
  I2C_4,     /**< I2C4：PC6 SCL / PC7 SDA */
  I2C_NUM
} i2c_index_enum;

/** @brief I2C 初始化参数。 */
typedef struct {
  uint32_t clock_speed_hz; /**< 总线速率，如 100000 / 400000。 */
  uint32_t own_address;    /**< 本机 7 位地址，0 表示从机模式不关心。 */
} i2c_cfg_t;

/**
 * @brief 初始化 I2C，并按目标速率计算 TIMINGR。
 * @param bus I2C 逻辑编号。
 * @param cfg 配置参数；传 NULL 使用 bsp_config.h 默认值（400 kHz）。
 * @return ZF_OK 表示成功，其他值表示失败。
 * @note TIMINGR 由本函数根据 PCLK1 与目标速率实时计算。
 */
zf_status_t i2c_init(i2c_index_enum bus, const i2c_cfg_t *cfg);

/**
 * @brief 向 7 位地址设备写数据。
 * @param bus I2C 逻辑编号。
 * @param address 7 位设备地址（不含读写位）。
 * @param data 待写缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t i2c_write(i2c_index_enum bus, uint8_t address,
                           const uint8_t *data, uint16_t length,
                           uint32_t timeout_ms);

/**
 * @brief 从 7 位地址设备读数据。
 * @param bus I2C 逻辑编号。
 * @param address 7 位设备地址（不含读写位）。
 * @param data 接收缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t i2c_read(i2c_index_enum bus, uint8_t address,
                          uint8_t *data, uint16_t length, uint32_t timeout_ms);

/**
 * @brief 写设备内部寄存器。
 * @param bus I2C 逻辑编号。
 * @param address 7 位设备地址。
 * @param mem_address 寄存器地址。
 * @param mem_address_size 寄存器地址字节数（I2C_MEMADD_SIZE_8BIT/16BIT）。
 * @param data 待写缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t i2c_mem_write(i2c_index_enum bus, uint8_t address,
                              uint16_t mem_address, uint16_t mem_address_size,
                              const uint8_t *data, uint16_t length,
                              uint32_t timeout_ms);

/**
 * @brief 读设备内部寄存器。
 * @param bus I2C 逻辑编号。
 * @param address 7 位设备地址。
 * @param mem_address 寄存器地址。
 * @param mem_address_size 寄存器地址字节数（I2C_MEMADD_SIZE_8BIT/16BIT）。
 * @param data 接收缓冲区。
 * @param length 字节数。
 * @param timeout_ms 超时时间，单位毫秒。
 * @return ZF_OK 表示成功，其他值表示失败。
 */
zf_status_t i2c_mem_read(i2c_index_enum bus, uint8_t address,
                             uint16_t mem_address, uint16_t mem_address_size,
                             uint8_t *data, uint16_t length,
                             uint32_t timeout_ms);

/**
 * @brief 探测设备是否应答。
 * @param bus I2C 逻辑编号。
 * @param address 7 位设备地址。
 * @param trials 尝试次数。
 * @param timeout_ms 每次尝试的超时时间，单位毫秒。
 * @return ZF_OK 表示设备应答，其他值表示失败。
 */
zf_status_t i2c_is_device_ready(i2c_index_enum bus, uint8_t address,
                                   uint32_t trials, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* _zf_driver_i2c_h_ */
