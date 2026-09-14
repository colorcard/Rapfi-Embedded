#ifndef _rp_device_w25q128_h_
#define _rp_device_w25q128_h_

#include <stdbool.h>
#include <stdint.h>

#include "rp_common_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief JEDEC ID 字节数。 */
#define W25Q128_JEDEC_ID_SIZE  3U
/** @brief 扇区大小，单位字节（4KB）。 */
#define W25Q128_SECTOR_SIZE    4096U
/** @brief 页大小，单位字节（256B）。 */
#define W25Q128_PAGE_SIZE      256U
/** @brief W25Q128 厂商 ID（Winbond）。 */
#define W25Q128_MANUFACTURER   0xEFU

/**
 * @brief 初始化 W25Q128 所用 SPI2 与片选引脚。
 * @return RP_OK 表示成功，其他值表示失败。
 * @note 本器件挂在 SPI2：PB13=SCK / PB14=MISO / PB15=MOSI / PB12=CS。
 *       PB12/PB13 与拓展板 FDCAN2、PB14/PB15 与 ADC 电源采样复用，
 *       使用期间会占用这些引脚。
 */
rp_status_t w25q128_init(void);

/**
 * @brief 读取 JEDEC ID（厂商 + 存储类型 + 容量）。
 * @param id 输出缓冲区，长度至少 W25Q128_JEDEC_ID_SIZE。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t w25q128_read_jedec_id(uint8_t id[W25Q128_JEDEC_ID_SIZE]);

/**
 * @brief 探测器件是否存在。
 * @return true 表示读到 Winbond 厂商 ID（0xEF）。
 * @note 需先调用 w25q128_init()。
 */
bool w25q128_is_connected(void);

/**
 * @brief 从指定地址读取任意长度数据（命令 0x03）。
 * @param address 起始地址。
 * @param data 输出缓冲区。
 * @param length 字节数。
 * @return RP_OK 表示成功，其他值表示失败。
 */
rp_status_t w25q128_read(uint32_t address, uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* _rp_device_w25q128_h_ */
