#include "rp_device_w25q128.h"

#include "rp_driver_gpio.h"
#include "rp_driver_spi.h"

/** @brief W25Q128 所用 SPI 逻辑编号。 */
#define W25Q128_SPI          SPI_2
/** @brief 片选引脚。 */
#define W25Q128_CS_PORT      GPIOB
#define W25Q128_CS_PIN       GPIO_PIN_12
/** @brief 单次 SPI 传输超时，单位毫秒。 */
#define W25Q128_TIMEOUT_MS   100U

/** @brief 命令：读 JEDEC ID。 */
#define W25Q128_CMD_JEDEC_ID 0x9FU
/** @brief 命令：读数据。 */
#define W25Q128_CMD_READ     0x03U

/**
 * @brief 控制片选电平。
 * @param level 非零拉低选中，零拉高释放。
 * @return 无。
 */
static void w25q128_cs(uint8_t level)
{
  gpio_set_level(W25Q128_CS_PORT, W25Q128_CS_PIN,
                 (level != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

rp_status_t w25q128_init(void)
{
  const gpio_cfg_t cs_cfg = {
    W25Q128_CS_PORT, W25Q128_CS_PIN, GPIO_MODE_OUTPUT_PP,
    GPIO_NOPULL, GPIO_SPEED_FREQ_VERY_HIGH, 0U
  };

  gpio_init(&cs_cfg);
  w25q128_cs(0U);
  return spi_init(W25Q128_SPI, NULL);
}

rp_status_t w25q128_read_jedec_id(uint8_t id[W25Q128_JEDEC_ID_SIZE])
{
  uint8_t tx[4];
  uint8_t rx[4] = {0U};
  rp_status_t status;

  if (id == NULL) {
    return RP_INVALID_PARAM;
  }

  tx[0] = W25Q128_CMD_JEDEC_ID;
  tx[1] = 0xFFU;
  tx[2] = 0xFFU;
  tx[3] = 0xFFU;

  w25q128_cs(1U);
  status = spi_transfer_8bit(W25Q128_SPI, tx, rx, sizeof(tx), W25Q128_TIMEOUT_MS);
  w25q128_cs(0U);

  if (status != RP_OK) {
    return status;
  }
  id[0] = rx[1];
  id[1] = rx[2];
  id[2] = rx[3];
  return RP_OK;
}

bool w25q128_is_connected(void)
{
  uint8_t id[W25Q128_JEDEC_ID_SIZE] = {0U};

  if (w25q128_read_jedec_id(id) != RP_OK) {
    return false;
  }
  return id[0] == W25Q128_MANUFACTURER;
}

rp_status_t w25q128_read(uint32_t address, uint8_t *data, uint16_t length)
{
  uint8_t command[4];
  uint8_t dummy = 0xFFU;
  uint16_t i;
  rp_status_t status = RP_OK;

  if ((data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }

  command[0] = W25Q128_CMD_READ;
  command[1] = (uint8_t)(address >> 16);
  command[2] = (uint8_t)(address >> 8);
  command[3] = (uint8_t)address;

  w25q128_cs(1U);
  status = spi_write_8bit_array(W25Q128_SPI, command, sizeof(command),
                                W25Q128_TIMEOUT_MS);
  for (i = 0U; (i < length) && (status == RP_OK); ++i) {
    status = spi_transfer_8bit(W25Q128_SPI, &dummy, &data[i], 1U,
                               W25Q128_TIMEOUT_MS);
  }
  w25q128_cs(0U);
  return status;
}
