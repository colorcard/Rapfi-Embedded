#include "rp_device_nrf24l01.h"

#include "rp_driver_gpio.h"
#include "rp_driver_spi.h"

/** @brief NRF24L01 片选引脚（低有效）。 */
#define NRF24L01_CSN_PORT GPIOB
#define NRF24L01_CSN_PIN  GPIO_PIN_1
/** @brief NRF24L01 使能引脚。 */
#define NRF24L01_CE_PORT  GPIOB
#define NRF24L01_CE_PIN   GPIO_PIN_2
/** @brief NRF24L01 使用的 SPI 总线与传输超时。 */
#define NRF24L01_SPI_BUS      SPI_1
#define NRF24L01_TIMEOUT_MS   100U
/** @brief 寄存器地址在命令字中的有效位数。 */
#define NRF24L01_REG_MASK     0x1FU

void nrf24l01_ce(uint8_t level)
{
  gpio_set_level(NRF24L01_CE_PORT, NRF24L01_CE_PIN,
                 (level != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void nrf24l01_csn(uint8_t level)
{
  gpio_set_level(NRF24L01_CSN_PORT, NRF24L01_CSN_PIN,
                 (level != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief 发送一个单字节命令（CSN 拉低 -> 命令 -> CSN 拉高）。
 * @param command 命令字。
 * @return RP_OK 表示成功，其他值表示失败。
 */
static rp_status_t nrf24l01_send_command(uint8_t command)
{
  rp_status_t status;

  nrf24l01_csn(0U);
  status = spi_write_8bit_array(NRF24L01_SPI_BUS, &command, 1U,
                                NRF24L01_TIMEOUT_MS);
  nrf24l01_csn(1U);
  return status;
}

rp_status_t nrf24l01_init(void)
{
  const gpio_cfg_t config = {
    NRF24L01_CSN_PORT, NRF24L01_CSN_PIN | NRF24L01_CE_PIN,
    GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, 0U
  };
  rp_status_t status;

  /* PB1/PB2：片选与使能，先置为待机状态。 */
  gpio_init(&config);
  nrf24l01_csn(1U);
  nrf24l01_ce(0U);

  status = spi_init(NRF24L01_SPI_BUS, NULL);
  if (status != RP_OK) {
    return status;
  }
  return spi_set_prescaler(NRF24L01_SPI_BUS, SPI_BAUDRATEPRESCALER_32);
}

rp_status_t nrf24l01_write_reg(uint8_t reg, uint8_t value)
{
  uint8_t buffer[2];
  rp_status_t status;

  buffer[0] = (uint8_t)(NRF24L01_W_REGISTER | (reg & NRF24L01_REG_MASK));
  buffer[1] = value;
  nrf24l01_csn(0U);
  status = spi_write_8bit_array(NRF24L01_SPI_BUS, buffer, 2U,
                                NRF24L01_TIMEOUT_MS);
  nrf24l01_csn(1U);
  return status;
}

rp_status_t nrf24l01_read_reg(uint8_t reg, uint8_t *value)
{
  uint8_t command;
  uint8_t status_byte;
  const uint8_t dummy = NRF24L01_NOP;
  rp_status_t status;

  if (value == NULL) {
    return RP_INVALID_PARAM;
  }

  command = (uint8_t)(NRF24L01_R_REGISTER | (reg & NRF24L01_REG_MASK));
  nrf24l01_csn(0U);
  status = spi_transfer_8bit(NRF24L01_SPI_BUS, &command, &status_byte, 1U,
                             NRF24L01_TIMEOUT_MS);
  if (status == RP_OK) {
    status = spi_transfer_8bit(NRF24L01_SPI_BUS, &dummy, value, 1U,
                               NRF24L01_TIMEOUT_MS);
  }
  nrf24l01_csn(1U);
  return status;
}

rp_status_t nrf24l01_write_buffer(uint8_t reg, const uint8_t *data,
                                  uint8_t length)
{
  uint8_t command;
  rp_status_t status;

  if ((data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }

  command = (uint8_t)(NRF24L01_W_REGISTER | (reg & NRF24L01_REG_MASK));
  nrf24l01_csn(0U);
  status = spi_write_8bit_array(NRF24L01_SPI_BUS, &command, 1U,
                                NRF24L01_TIMEOUT_MS);
  if (status == RP_OK) {
    status = spi_write_8bit_array(NRF24L01_SPI_BUS, data, length,
                                  NRF24L01_TIMEOUT_MS);
  }
  nrf24l01_csn(1U);
  return status;
}

rp_status_t nrf24l01_read_buffer(uint8_t reg, uint8_t *data, uint8_t length)
{
  uint8_t command;
  uint8_t status_byte;
  rp_status_t status;

  if ((data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }

  command = (uint8_t)(NRF24L01_R_REGISTER | (reg & NRF24L01_REG_MASK));
  nrf24l01_csn(0U);
  status = spi_transfer_8bit(NRF24L01_SPI_BUS, &command, &status_byte, 1U,
                             NRF24L01_TIMEOUT_MS);
  if (status == RP_OK) {
    status = spi_read_8bit_array(NRF24L01_SPI_BUS, data, length,
                                 NRF24L01_TIMEOUT_MS);
  }
  nrf24l01_csn(1U);
  return status;
}

rp_status_t nrf24l01_write_tx_payload(const uint8_t *data, uint8_t length)
{
  const uint8_t command = NRF24L01_W_TX_PAYLOAD;
  rp_status_t status;

  if ((data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }

  nrf24l01_csn(0U);
  status = spi_write_8bit_array(NRF24L01_SPI_BUS, &command, 1U,
                                NRF24L01_TIMEOUT_MS);
  if (status == RP_OK) {
    status = spi_write_8bit_array(NRF24L01_SPI_BUS, data, length,
                                  NRF24L01_TIMEOUT_MS);
  }
  nrf24l01_csn(1U);
  return status;
}

rp_status_t nrf24l01_read_rx_payload(uint8_t *data, uint8_t length)
{
  const uint8_t command = NRF24L01_R_RX_PAYLOAD;
  uint8_t status_byte;
  rp_status_t status;

  if ((data == NULL) || (length == 0U)) {
    return RP_INVALID_PARAM;
  }

  nrf24l01_csn(0U);
  status = spi_transfer_8bit(NRF24L01_SPI_BUS, &command, &status_byte, 1U,
                             NRF24L01_TIMEOUT_MS);
  if (status == RP_OK) {
    status = spi_read_8bit_array(NRF24L01_SPI_BUS, data, length,
                                 NRF24L01_TIMEOUT_MS);
  }
  nrf24l01_csn(1U);
  return status;
}

rp_status_t nrf24l01_flush_tx(void)
{
  return nrf24l01_send_command(NRF24L01_FLUSH_TX);
}

rp_status_t nrf24l01_flush_rx(void)
{
  return nrf24l01_send_command(NRF24L01_FLUSH_RX);
}
