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
/** @brief 每个输出条带的行数。 */
#define VIDEO_STRIP_ROWS  16U
/** @brief 单条带的输出像素数。 */
#define VIDEO_STRIP_PX    (LCD_WIDTH * VIDEO_STRIP_ROWS)
/** @brief 帧头魔数首字节；次字节 0x5A..0x5D 选择格式。 */
#define VIDEO_MAGIC_0     0xA5U
/** @brief 视频判定窗口：收到帧后多久内视为“正在推流”。 */
#define VIDEO_ACTIVE_MS   300U
/** @brief 支持的帧格式数量（次字节 0x5A + idx）。 */
#define VIDEO_FMT_COUNT   4U

/**
 * @brief 帧格式描述。
 * bpp=2 表示 RGB565（大端），bpp=1 表示 RGB332。
 * scale 表示 MCU 侧对其做整数倍放大（1=原分辨率，2=放大到整屏）。
 */
typedef struct {
  uint8_t bpp;
  uint16_t src_w;
  uint16_t scale;
} video_fmt_t;

/** @brief A5 5A / A5 5B / A5 5C / A5 5D 对应的格式。 */
static const video_fmt_t s_fmt[VIDEO_FMT_COUNT] = {
  {2U, LCD_WIDTH, 1U},        /* A5 5A: RGB565 280x240 */
  {1U, LCD_WIDTH, 1U},        /* A5 5B: RGB332 280x240 */
  {1U, LCD_WIDTH / 2U, 2U},   /* A5 5C: RGB332 140x120 -> 2x */
  {2U, LCD_WIDTH / 2U, 2U},   /* A5 5D: RGB565 140x120 -> 2x */
};

static uint8_t s_rx_fifo[USB_RX_FIFO_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

static uint8_t s_src[VIDEO_STRIP_PX * 2U]; /* 源条带缓冲（最大=RGB565 满宽）*/
static uint8_t s_dst[VIDEO_STRIP_PX * 2U]; /* 展开后的输出条带（RGB565 大端）*/
static uint16_t s_src_target;
static uint16_t s_src_fill;
static uint16_t s_frame_row;
static uint8_t s_video_state;
static uint8_t s_fmt_index;
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
 * @brief 把当前源条带展开/放大为整行宽 280、16 行的 RGB565 大端输出。
 * @return 无。
 */
static void video_expand(void)
{
  const video_fmt_t *fmt = &s_fmt[s_fmt_index];
  uint16_t oy;

  for (oy = 0U; oy < VIDEO_STRIP_ROWS; ++oy) {
    uint16_t sy = (uint16_t)(oy / fmt->scale);
    uint16_t ox;
    for (ox = 0U; ox < LCD_WIDTH; ++ox) {
      uint16_t sx = (uint16_t)(ox / fmt->scale);
      uint32_t si = ((uint32_t)sy * fmt->src_w + sx) * (uint32_t)fmt->bpp;
      uint16_t col;
      if (fmt->bpp == 2U) {
        col = (uint16_t)(((uint16_t)s_src[si] << 8) | s_src[si + 1U]);
      } else {
        uint8_t c = s_src[si];
        uint8_t r3 = (uint8_t)((c >> 5) & 7U);
        uint8_t g3 = (uint8_t)((c >> 2) & 7U);
        uint8_t b2 = (uint8_t)(c & 3U);
        col = (uint16_t)(((uint16_t)((r3 << 2) | (r3 >> 1)) << 11)
                         | ((uint16_t)((g3 << 3) | g3) << 5)
                         | (uint16_t)((b2 << 3) | (b2 << 1) | (b2 >> 1)));
      }
      {
        uint32_t di = ((uint32_t)oy * LCD_WIDTH + ox) * 2U;
        s_dst[di] = (uint8_t)(col >> 8);
        s_dst[di + 1U] = (uint8_t)col;
      }
    }
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
      /* 等待帧头：A5 5A..5D。 */
      if (s_magic == 0U) {
        if (byte == VIDEO_MAGIC_0) {
          s_magic = 1U;
        }
      } else if ((byte >= 0x5AU) && (byte < (0x5AU + VIDEO_FMT_COUNT))) {
        const video_fmt_t *fmt = &s_fmt[byte - 0x5AU];
        uint16_t src_rows = (uint16_t)(VIDEO_STRIP_ROWS / fmt->scale);
        s_fmt_index = (uint8_t)(byte - 0x5AU);
        s_src_target = (uint16_t)(fmt->src_w * src_rows * fmt->bpp);
        s_video_state = 1U;
        s_frame_row = 0U;
        s_src_fill = 0U;
        s_magic = 0U;
      } else {
        s_magic = (byte == VIDEO_MAGIC_0) ? 1U : 0U;
      }
      continue;
    }

    s_src[s_src_fill] = byte;
    ++s_src_fill;

    if (s_src_fill >= s_src_target) {
      video_expand();
      if (lcd_hw_start_area(0U, s_frame_row, LCD_WIDTH, VIDEO_STRIP_ROWS) == 0) {
        (void)spi_write_8bit_array(SPI_1, s_dst,
                                   (uint16_t)(VIDEO_STRIP_PX * 2U), 1000U);
        lcd_hw_end_area();
      }
      ++g_video_strip_cnt;
      s_frame_row = (uint16_t)(s_frame_row + VIDEO_STRIP_ROWS);
      s_src_fill = 0U;
      if (s_frame_row >= LCD_HEIGHT) {
        s_video_state = 0U; /* 一帧结束，等待下一帧魔数 */
        s_last_frame_tick = HAL_GetTick();
        ++g_video_frame_cnt;
      }
    }
  }
}
