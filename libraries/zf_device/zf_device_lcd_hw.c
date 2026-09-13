#include "zf_device_lcd_hw.h"

#include "zf_common_bsp_config.h"
#include "zf_driver_gpio.h"
#include "zf_driver_spi.h"

#if (BSP_ENABLE_SPI1 == 0U)
#error "LCD requires BSP_ENABLE_SPI1=1 in zf_common_bsp_config.h"
#endif

#if (LCD_BL_ACTIVE_HIGH != 0U)
#define LCD_BL_ON_LEVEL  GPIO_PIN_SET
#define LCD_BL_OFF_LEVEL GPIO_PIN_RESET
#else
#define LCD_BL_ON_LEVEL  GPIO_PIN_RESET
#define LCD_BL_OFF_LEVEL GPIO_PIN_SET
#endif

/**
 * @brief 设置 LCD 片选信号电平
 * @param state 要输出的 GPIO 电平
 */
static void lcd_cs(GPIO_PinState state)
{
  gpio_set_level(LCD_CS_PORT, LCD_CS_PIN, state);
}

/**
 * @brief 通过 SPI 向 LCD 写入命令字节或数据字节
 * @param is_data 非零表示数据，零表示命令
 * @param data 待发送字节缓冲区
 * @param size 待发送字节数
 * @return HAL_OK 发送成功，其他值表示发送失败
 */
static HAL_StatusTypeDef lcd_write(uint8_t is_data, const uint8_t *data, uint16_t size)
{
  HAL_StatusTypeDef status;

  if (data == NULL || size == 0U) return HAL_ERROR;
  lcd_cs(GPIO_PIN_RESET);
  gpio_set_level(LCD_CS_PORT, LCD_DC_PIN, is_data ? GPIO_PIN_SET : GPIO_PIN_RESET);
  status = (spi_write_8bit_array(LCD_SPI_INDEX, data, size, LCD_TIMEOUT_MS) == ZF_OK)
               ? HAL_OK : HAL_ERROR;
  lcd_cs(GPIO_PIN_SET);
  return status;
}

/**
 * @brief 向 LCD 发送一条命令及其可选参数
 * @param command LCD 控制器命令码
 * @param data 命令参数缓冲区，无参数时可为 NULL
 * @param size 命令参数字节数
 * @return HAL_OK 发送成功，其他值表示发送失败
 */
static HAL_StatusTypeDef lcd_command(uint8_t command, const uint8_t *data, uint16_t size)
{
  HAL_StatusTypeDef status = lcd_write(0U, &command, 1U);
  if (status == HAL_OK && size != 0U) status = lcd_write(1U, data, size);
  return status;
}

/**
 * @brief 设置 LCD 控制器的连续显存写入窗口
 * @param x 窗口左上角横坐标
 * @param y 窗口左上角纵坐标
 * @param width 窗口宽度，单位为像素
 * @param height 窗口高度，单位为像素
 * @return HAL_OK 设置成功，其他值表示通信失败
 */
/**
 * @brief 设置数据/命令选择引脚。
 * @param is_data 非零表示数据，零表示命令。
 * @return 无。
 */
static void lcd_dc(uint8_t is_data)
{
  gpio_set_level(LCD_CS_PORT, LCD_DC_PIN,
                 (is_data != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief 在片选保持期间发送命令或数据。
 * @param is_data 非零表示数据，零表示命令。
 * @param data 数据缓冲区。
 * @param size 字节数。
 * @return HAL_OK 表示成功。
 */
static HAL_StatusTypeDef lcd_tx_held(uint8_t is_data, const uint8_t *data,
                                     uint16_t size)
{
  lcd_dc(is_data);
  return (spi_write_8bit_array(LCD_SPI_INDEX, data, size, LCD_TIMEOUT_MS)
          == ZF_OK) ? HAL_OK : HAL_ERROR;
}

int lcd_hw_start_area(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
  uint16_t x2 = (uint16_t)(x + width - 1U);
  uint16_t y2 = (uint16_t)(y + height - 1U);
  uint16_t caset1, caset2, raset1, raset2;
  uint8_t area[4];
  uint8_t command;

  if ((width == 0U) || (height == 0U)) {
    return -1;
  }

#if ((LCD_MADCTL & 0x20U) != 0U)
  /* 横屏（MV）：CASET 为长轴（行）地址，由 x 驱动并带 20 行偏移；RASET 为列地址。 */
  caset1 = (uint16_t)(x + LCD_Y_OFFSET);
  caset2 = (uint16_t)(x2 + LCD_Y_OFFSET);
  raset1 = (uint16_t)(y + LCD_X_OFFSET);
  raset2 = (uint16_t)(y2 + LCD_X_OFFSET);
#else
  caset1 = (uint16_t)(x + LCD_X_OFFSET);
  caset2 = (uint16_t)(x2 + LCD_X_OFFSET);
  raset1 = (uint16_t)(y + LCD_Y_OFFSET);
  raset2 = (uint16_t)(y2 + LCD_Y_OFFSET);
#endif

  lcd_cs(GPIO_PIN_RESET);

  command = 0x2AU;
  area[0] = (uint8_t)(caset1 >> 8); area[1] = (uint8_t)caset1;
  area[2] = (uint8_t)(caset2 >> 8); area[3] = (uint8_t)caset2;
  if ((lcd_tx_held(0U, &command, 1U) != HAL_OK) ||
      (lcd_tx_held(1U, area, sizeof(area)) != HAL_OK)) {
    lcd_cs(GPIO_PIN_SET);
    return -1;
  }

  command = 0x2BU;
  area[0] = (uint8_t)(raset1 >> 8); area[1] = (uint8_t)raset1;
  area[2] = (uint8_t)(raset2 >> 8); area[3] = (uint8_t)raset2;
  if ((lcd_tx_held(0U, &command, 1U) != HAL_OK) ||
      (lcd_tx_held(1U, area, sizeof(area)) != HAL_OK)) {
    lcd_cs(GPIO_PIN_SET);
    return -1;
  }

  command = 0x2CU;
  if (lcd_tx_held(0U, &command, 1U) != HAL_OK) {
    lcd_cs(GPIO_PIN_SET);
    return -1;
  }

  lcd_dc(1U); /* 后续为像素数据。 */
  return 0;
}

int lcd_hw_send_pixels_dma(const void *pixels, uint32_t length)
{
  if ((pixels == NULL) || (length == 0U) || (length > 0xFFFFU)) {
    return -1;
  }
  lcd_dc(1U);
  return (spi_write_dma(LCD_SPI_INDEX, (const uint8_t *)pixels,
                        (uint16_t)length) == ZF_OK) ? 0 : -1;
}

void lcd_hw_end_area(void)
{
  lcd_cs(GPIO_PIN_SET);
}

/**
 * @brief 初始化 ST7789 控制器并开启 LCD 背光
 * @return 0 初始化成功，-1 表示 SPI 通信失败
 */
int lcd_hw_init(void)
{
  static const uint8_t b2[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
  static const uint8_t d0[] = {0xA4U, 0xA1U};
  static const uint8_t e0[] = {0xD0U,0x04U,0x0DU,0x11U,0x13U,0x2BU,0x3FU,
                               0x54U,0x4CU,0x18U,0x0DU,0x0BU,0x1FU,0x23U};
  static const uint8_t e1[] = {0xD0U,0x04U,0x0CU,0x11U,0x13U,0x2CU,0x3FU,
                               0x44U,0x51U,0x2FU,0x1FU,0x1FU,0x20U,0x23U};
  static const struct { uint8_t command; uint8_t value; } single[] = {
    {0x36U,LCD_MADCTL}, {0x3AU,0x05U}, {0xB7U,0x35U}, {0xBBU,0x19U},
    {0xC0U,0x2CU}, {0xC2U,0x01U}, {0xC3U,0x12U}, {0xC4U,0x20U},
    {0xC6U,0x0FU}
  };
  const gpio_cfg_t gpio_config = {
    LCD_CS_PORT, LCD_CS_PIN | LCD_DC_PIN | LCD_BL_PIN, GPIO_MODE_OUTPUT_PP,
    GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH, 0U
  };
  uint32_t i;

  /* PD11/PD12/PD13：LCD 片选、数据/命令、背光控制。 */
  gpio_init(&gpio_config);

  lcd_cs(GPIO_PIN_SET);
  gpio_set_level(LCD_CS_PORT, LCD_DC_PIN, GPIO_PIN_SET);
  gpio_set_level(LCD_CS_PORT, LCD_BL_PIN, LCD_BL_OFF_LEVEL);

  for (i = 0U; i < sizeof(single) / sizeof(single[0]); ++i) {
    if (lcd_command(single[i].command, &single[i].value, 1U) != HAL_OK) return -1;
  }
  if (lcd_command(0xB2U, b2, sizeof(b2)) != HAL_OK ||
      lcd_command(0xD0U, d0, sizeof(d0)) != HAL_OK ||
      lcd_command(0xE0U, e0, sizeof(e0)) != HAL_OK ||
      lcd_command(0xE1U, e1, sizeof(e1)) != HAL_OK ||
      lcd_command(0x21U, NULL, 0U) != HAL_OK ||
      lcd_command(0x11U, NULL, 0U) != HAL_OK) return -1;
  HAL_Delay(120U);
  if (lcd_command(0x29U, NULL, 0U) != HAL_OK) return -1;
  gpio_set_level(LCD_CS_PORT, LCD_BL_PIN, LCD_BL_ON_LEVEL);
  return 0;
}

/**
 * @brief 将 RGB565 像素块写入 LCD 指定区域
 * @param x 目标区域左上角横坐标
 * @param y 目标区域左上角纵坐标
 * @param width 目标区域宽度，单位为像素
 * @param height 目标区域高度，单位为像素
 * @param pixels 按行连续排列的 RGB565 像素数据
 * @return 0 写入成功，-1 表示参数非法或 SPI 通信失败
 */
int lcd_hw_write_area_rgb565(uint16_t x, uint16_t y, uint16_t width,
                           uint16_t height, const uint16_t *pixels)
{
  /* 行缓冲放在静态区，避免占用主循环栈空间。 */
  static uint8_t row_data[LCD_HW_WIDTH * 2U];
  uint16_t row;
  uint16_t column;
  uint16_t x2;
  uint16_t y2;
  uint16_t caset1, caset2, raset1, raset2;
  uint8_t area[4];

  if (pixels == NULL || width == 0U || height == 0U ||
      x >= LCD_HW_WIDTH || y >= LCD_HW_HEIGHT ||
      width > LCD_HW_WIDTH - x || height > LCD_HW_HEIGHT - y) return -1;

  x2 = (uint16_t)(x + width - 1U);
  y2 = (uint16_t)(y + height - 1U);
#if ((LCD_MADCTL & 0x20U) != 0U)
  caset1 = (uint16_t)(x + LCD_Y_OFFSET);
  caset2 = (uint16_t)(x2 + LCD_Y_OFFSET);
  raset1 = (uint16_t)(y + LCD_X_OFFSET);
  raset2 = (uint16_t)(y2 + LCD_X_OFFSET);
#else
  caset1 = (uint16_t)(x + LCD_X_OFFSET);
  caset2 = (uint16_t)(x2 + LCD_X_OFFSET);
  raset1 = (uint16_t)(y + LCD_Y_OFFSET);
  raset2 = (uint16_t)(y2 + LCD_Y_OFFSET);
#endif

  area[0] = (uint8_t)(caset1 >> 8); area[1] = (uint8_t)caset1;
  area[2] = (uint8_t)(caset2 >> 8); area[3] = (uint8_t)caset2;
  if (lcd_command(0x2AU, area, sizeof(area)) != HAL_OK) return -1;

  area[0] = (uint8_t)(raset1 >> 8); area[1] = (uint8_t)raset1;
  area[2] = (uint8_t)(raset2 >> 8); area[3] = (uint8_t)raset2;
  if (lcd_command(0x2BU, area, sizeof(area)) != HAL_OK) return -1;
  if (lcd_command(0x2CU, NULL, 0U) != HAL_OK) return -1;

  for (row = 0U; row < height; ++row) {
    for (column = 0U; column < width; ++column) {
      uint16_t color = pixels[(uint32_t)row * width + column];
      row_data[column * 2U] = (uint8_t)(color >> 8);
      row_data[column * 2U + 1U] = (uint8_t)color;
    }
    if (lcd_write(1U, row_data, (uint16_t)(width * 2U)) != HAL_OK) return -1;
  }
  return 0;
}
