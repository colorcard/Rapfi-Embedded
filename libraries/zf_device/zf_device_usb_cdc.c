#include "zf_device_usb_cdc.h"

#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_cdc_if.h"

#include "zf_common_bsp_config.h"
#include "zf_device_lcd_hw.h"
#include "zf_driver_spi.h"

/** @brief USB 设备句柄（供 CDC 接口层引用）。 */
USBD_HandleTypeDef hUsbDeviceFS;
/** @brief PCD 句柄（定义在 usbd_conf.c）。 */
extern PCD_HandleTypeDef hpcd_USB_FS;

/** @brief 接收环形缓冲大小。 */
#define USB_RX_FIFO_SIZE  16384U
/** @brief 视频判定窗口：收到帧后多久内视为“正在推流”。 */
#define VIDEO_ACTIVE_MS   300U
/** @brief 视频帧的条带行数。 */
#define VIDEO_STRIP_ROWS  16U
/** @brief 视频帧魔数（A5 5A 开头）。 */
#define VIDEO_MAGIC_0     0xA5U
#define VIDEO_MAGIC_1     0x5AU

static uint8_t s_rx_fifo[USB_RX_FIFO_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

static uint8_t s_strip[LCD_WIDTH * VIDEO_STRIP_ROWS * 2U];
static uint16_t s_strip_fill;
static uint16_t s_frame_row;
static uint8_t s_video_state;
static uint8_t s_magic;
static volatile uint32_t s_last_frame_tick;

void zf_usb_cdc_on_rx(const uint8_t *data, uint32_t len)
{
  uint32_t i;

  if (data == NULL) {
    return;
  }
  for (i = 0U; i < len; ++i) {
    uint32_t next = (s_rx_head + 1U) % USB_RX_FIFO_SIZE;
    if (next == s_rx_tail) {
      break; /* 缓冲已满，丢弃剩余数据 */
    }
    s_rx_fifo[s_rx_head] = data[i];
    s_rx_head = next;
  }
}

/**
 * @brief 从环形缓冲取一个字节。
 * @return 1 表示取到，0 表示空。
 */
static int usb_rx_get(uint8_t *byte)
{
  if (s_rx_tail == s_rx_head) {
    return 0;
  }
  *byte = s_rx_fifo[s_rx_tail];
  s_rx_tail = (s_rx_tail + 1U) % USB_RX_FIFO_SIZE;
  return 1;
}

uint32_t usb_cdc_read(uint8_t *data, uint32_t max_length)
{
  uint32_t count = 0U;

  if (data == NULL) {
    return 0U;
  }
  while ((count < max_length) && (usb_rx_get(&data[count]) != 0)) {
    ++count;
  }
  return count;
}

zf_status_t usb_cdc_init(void)
{
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK) {
    return ZF_ERROR;
  }
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK) {
    return ZF_ERROR;
  }
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) !=
      USBD_OK) {
    return ZF_ERROR;
  }
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
    return ZF_ERROR;
  }
  return ZF_OK;
}

void usb_cdc_irq_handler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
}

bool usb_cdc_video_active(void)
{
  return (uint32_t)(HAL_GetTick() - s_last_frame_tick) < VIDEO_ACTIVE_MS;
}

void usb_cdc_task(void)
{
  uint8_t byte;

  while (usb_rx_get(&byte) != 0) {
    if (s_video_state == 0U) {
      /* 等待帧头魔数 A5 5A。 */
      if (s_magic == 0U) {
        if (byte == VIDEO_MAGIC_0) {
          s_magic = 1U;
        }
      } else if (byte == VIDEO_MAGIC_1) {
        s_video_state = 1U;
        s_frame_row = 0U;
        s_strip_fill = 0U;
        s_magic = 0U;
      } else {
        s_magic = (byte == VIDEO_MAGIC_0) ? 1U : 0U;
      }
      continue;
    }

    s_strip[s_strip_fill] = byte;
    ++s_strip_fill;
    if (s_strip_fill >= (uint16_t)sizeof(s_strip)) {
      /* 一条填满即写入对应的屏幕窗口（数据为高字节先发的 RGB565）。 */
      if (lcd_hw_start_area(0U, s_frame_row, LCD_WIDTH, VIDEO_STRIP_ROWS) == 0) {
        (void)spi_write_8bit_array(SPI_1, s_strip, (uint16_t)sizeof(s_strip),
                                   1000U);
        lcd_hw_end_area();
      }
      s_frame_row = (uint16_t)(s_frame_row + VIDEO_STRIP_ROWS);
      s_strip_fill = 0U;
      if (s_frame_row >= LCD_HEIGHT) {
        s_video_state = 0U; /* 一帧结束，等待下一帧魔数 */
        s_last_frame_tick = HAL_GetTick();
      }
    }
  }
}
