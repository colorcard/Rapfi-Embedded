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
/** @brief 视频帧的条带行数。 */
#define VIDEO_STRIP_ROWS  16U
/** @brief 单条带的像素数。 */
#define VIDEO_STRIP_PX    (LCD_WIDTH * VIDEO_STRIP_ROWS)
/** @brief 帧头魔数：A5 5A 为 RGB565，A5 5B 为 RGB332。 */
#define VIDEO_MAGIC_0     0xA5U
#define VIDEO_MAGIC_565   0x5AU
#define VIDEO_MAGIC_332   0x5BU
/** @brief 视频判定窗口：收到帧后多久内视为“正在推流”。 */
#define VIDEO_ACTIVE_MS   300U

static uint8_t s_rx_fifo[USB_RX_FIFO_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

static uint8_t s_strip[VIDEO_STRIP_PX * 2U]; /* RGB565 条带（展开目标） */
static uint8_t s_strip332[VIDEO_STRIP_PX];   /* RGB332 接收条带 */
static uint16_t s_strip_fill;
static uint16_t s_strip_target;
static uint16_t s_frame_row;
static uint8_t s_video_state;
static uint8_t s_video_format; /* 0=RGB565, 1=RGB332 */
static uint8_t s_magic;
static volatile uint32_t s_last_frame_tick;
/** @brief 调试用：完成的帧数 / 写入的条带数 / 收到的字节数（供 SWD 观测）。 */
volatile uint32_t g_video_frame_cnt;
volatile uint32_t g_video_strip_cnt;
volatile uint32_t g_video_rx_bytes;

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

/**
 * @brief 将 RGB332 条带展开为高字节先发的 RGB565，写入 s_strip。
 * @return 无。
 */
static void video_expand_rgb332(void)
{
  uint16_t i;

  for (i = 0U; i < VIDEO_STRIP_PX; ++i) {
    uint8_t c = s_strip332[i];
    uint8_t r3 = (uint8_t)((c >> 5) & 7U);
    uint8_t g3 = (uint8_t)((c >> 2) & 7U);
    uint8_t b2 = (uint8_t)(c & 3U);
    uint16_t col = (uint16_t)(((uint16_t)((r3 << 2) | (r3 >> 1)) << 11)
                              | ((uint16_t)((g3 << 3) | g3) << 5)
                              | (uint16_t)((b2 << 3) | (b2 << 1) | (b2 >> 1)));

    s_strip[2U * i] = (uint8_t)(col >> 8);
    s_strip[2U * i + 1U] = (uint8_t)col;
  }
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
    ++g_video_rx_bytes;
    if (s_video_state == 0U) {
      /* 等待帧头魔数 A5 5A(RGB565) 或 A5 5B(RGB332)。 */
      if (s_magic == 0U) {
        if (byte == VIDEO_MAGIC_0) {
          s_magic = 1U;
        }
      } else if ((byte == VIDEO_MAGIC_565) || (byte == VIDEO_MAGIC_332)) {
        s_video_format = (byte == VIDEO_MAGIC_332) ? 1U : 0U;
        s_strip_target = (s_video_format == 0U) ? (uint16_t)sizeof(s_strip)
                                                : (uint16_t)sizeof(s_strip332);
        s_video_state = 1U;
        s_frame_row = 0U;
        s_strip_fill = 0U;
        s_magic = 0U;
      } else {
        s_magic = (byte == VIDEO_MAGIC_0) ? 1U : 0U;
      }
      continue;
    }

    if (s_video_format == 0U) {
      s_strip[s_strip_fill] = byte;
    } else {
      s_strip332[s_strip_fill] = byte;
    }
    ++s_strip_fill;

    if (s_strip_fill >= s_strip_target) {
      if (s_video_format == 1U) {
        video_expand_rgb332();
      }
      /* 一条填满即写入对应的屏幕窗口（高字节先发）。 */
      if (lcd_hw_start_area(0U, s_frame_row, LCD_WIDTH, VIDEO_STRIP_ROWS) == 0) {
        (void)spi_write_8bit_array(SPI_1, s_strip,
                                   (uint16_t)(VIDEO_STRIP_PX * 2U), 1000U);
        lcd_hw_end_area();
      }
      ++g_video_strip_cnt;
      s_frame_row = (uint16_t)(s_frame_row + VIDEO_STRIP_ROWS);
      s_strip_fill = 0U;
      if (s_frame_row >= LCD_HEIGHT) {
        s_video_state = 0U; /* 一帧结束，等待下一帧魔数 */
        s_last_frame_tick = HAL_GetTick();
        ++g_video_frame_cnt;
      }
    }
  }
}
