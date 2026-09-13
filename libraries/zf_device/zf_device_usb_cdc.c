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
/** @brief 每个条带的行数。 */
#define VIDEO_STRIP_ROWS  16U
/** @brief 每条带的输出像素数。 */
#define VIDEO_STRIP_PX    (LCD_WIDTH * VIDEO_STRIP_ROWS)
/** @brief 条带总数（240/16=15）。 */
#define VIDEO_STRIP_NUM   (LCD_HEIGHT / VIDEO_STRIP_ROWS)
/** @brief 帧头魔数首字节；次字节 0x5A..0x5D 选择格式。 */
#define VIDEO_MAGIC_0     0xA5U
/** @brief 视频判定窗口。 */
#define VIDEO_ACTIVE_MS   300U
/** @brief 支持的帧格式数量。 */
#define VIDEO_FMT_COUNT   4U
/** @brief 整帧缓冲大小（按最大可缓冲帧 RGB332 全屏预留）。 */
#define VIDEO_FRAME_CAP   (LCD_WIDTH * LCD_HEIGHT)

/** @brief 帧格式描述：bpp=2 为 RGB565(大端)，1 为 RGB332；scale 为整数放大。 */
typedef struct {
  uint8_t bpp;
  uint16_t src_w;
  uint16_t scale;
} video_fmt_t;

static const video_fmt_t s_fmt[VIDEO_FMT_COUNT] = {
  {2U, LCD_WIDTH, 1U},        /* A5 5A: RGB565 280x240 */
  {1U, LCD_WIDTH, 1U},        /* A5 5B: RGB332 280x240 */
  {1U, LCD_WIDTH / 2U, 2U},   /* A5 5C: RGB332 140x120 -> 2x */
  {2U, LCD_WIDTH / 2U, 2U},   /* A5 5D: RGB565 140x120 -> 2x */
};

/* 帧协议（delta）：A5 <fmt> + u16 mask（bit i=条带 i 有变化）+ 各变化条带源数据 */
typedef enum {
  VIDEO_WAIT_MAGIC = 0,
  VIDEO_READ_MASK,
  VIDEO_READ_DATA
} video_state_enum;

static uint8_t s_rx_fifo[USB_RX_FIFO_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

static uint8_t s_frame[VIDEO_FRAME_CAP]; /* 持久整帧缓冲（RGB332 或 RGB565-BE） */
static uint8_t s_dst[VIDEO_STRIP_PX * 2U];/* 展开后的输出条带（RGB565 大端） */
static uint32_t s_strip_bytes;            /* 每条带源字节数 */
static uint32_t s_fill;                   /* 当前条带已收字节 */
static uint16_t s_mask;                   /* 本帧变化条带掩码 */
static uint8_t s_data_strip;              /* 当前接收的条带号 */
static uint8_t s_video_state;
static uint8_t s_fmt_index;
static uint8_t s_mask_fill;
static uint8_t s_magic;
static volatile uint32_t s_last_frame_tick;
/** @brief 调试用计数。 */
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

static int usb_rx_get(uint8_t *byte)
{
  if (s_rx_tail == s_rx_head) {
    return 0;
  }
  *byte = s_rx_fifo[s_rx_tail];
  s_rx_tail = (s_rx_tail + 1U) % USB_RX_FIFO_SIZE;
  return 1;
}

/** @brief 将 RGB332 或 RGB565(大端) 字节转为 RGB565 值。 */
static uint16_t video_pixel(const uint8_t *p, uint8_t bpp)
{
  if (bpp == 2U) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
  }
  {
    uint8_t c = p[0];
    uint8_t r3 = (uint8_t)((c >> 5) & 7U);
    uint8_t g3 = (uint8_t)((c >> 2) & 7U);
    uint8_t b2 = (uint8_t)(c & 3U);
    return (uint16_t)(((uint16_t)((r3 << 2) | (r3 >> 1)) << 11)
                      | ((uint16_t)((g3 << 3) | g3) << 5)
                      | (uint16_t)((b2 << 3) | (b2 << 1) | (b2 >> 1)));
  }
}

/** @brief 从整帧缓冲展开条带 strip 到 s_dst（不发送）。 */
static void video_expand_strip(uint8_t strip)
{
  const video_fmt_t *fmt = &s_fmt[s_fmt_index];
  uint16_t out_row = (uint16_t)(strip * VIDEO_STRIP_ROWS);
  uint16_t oy;

  for (oy = 0U; oy < VIDEO_STRIP_ROWS; ++oy) {
    uint16_t sy = (uint16_t)((out_row + oy) / fmt->scale);
    uint16_t ox;
    for (ox = 0U; ox < LCD_WIDTH; ++ox) {
      uint16_t sx = (uint16_t)(ox / fmt->scale);
      const uint8_t *base = &s_frame[(uint32_t)strip * s_strip_bytes];
      uint32_t si = ((uint32_t)(sy - (uint32_t)out_row / fmt->scale) * fmt->src_w
                     + sx) * (uint32_t)fmt->bpp;
      uint16_t col = video_pixel(&base[si], fmt->bpp);
      uint32_t di = ((uint32_t)oy * LCD_WIDTH + ox) * 2U;
      s_dst[di] = (uint8_t)(col >> 8);
      s_dst[di + 1U] = (uint8_t)col;
    }
  }
}

/**
 * @brief 只刷本帧发生变化的条带区间：
 *        取变化的最小/最大条带，用单个窗口把该区间连续刷完（静止帧不写）。
 */
static void video_present(void)
{
  uint8_t first = (uint8_t)VIDEO_STRIP_NUM;
  uint8_t last = 0U;
  uint8_t strip;

  if (s_mask == 0U) {
    return;
  }
  for (strip = 0U; strip < (uint8_t)VIDEO_STRIP_NUM; ++strip) {
    if ((s_mask & (uint16_t)(1U << strip)) != 0U) {
      if (first >= (uint8_t)VIDEO_STRIP_NUM) {
        first = strip;
      }
      last = strip;
    }
  }

  if (lcd_hw_start_area(0U, (uint16_t)(first * VIDEO_STRIP_ROWS), LCD_WIDTH,
                        (uint16_t)((last - first + 1U) * VIDEO_STRIP_ROWS))
      != 0) {
    return;
  }
  for (strip = first; strip <= last; ++strip) {
    video_expand_strip(strip);
    (void)spi_write_8bit_array(SPI_1, s_dst, (uint16_t)(VIDEO_STRIP_PX * 2U),
                               1000U);
    ++g_video_strip_cnt;
  }
  lcd_hw_end_area();
}

/**
 * @brief 在掩码中查找下一个置位条带（从 from 起）。
 * @return 条带号；没有则返回 VIDEO_STRIP_NUM。
 */
static uint8_t video_next_strip(uint8_t from)
{
  uint8_t s;

  for (s = from; s < VIDEO_STRIP_NUM; ++s) {
    if ((s_mask & (uint16_t)(1U << s)) != 0U) {
      return s;
    }
  }
  return (uint8_t)VIDEO_STRIP_NUM;
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
    switch (s_video_state) {
      case VIDEO_WAIT_MAGIC:
        if (s_magic == 0U) {
          if (byte == VIDEO_MAGIC_0) {
            s_magic = 1U;
          }
        } else if ((byte >= 0x5AU) && (byte < (0x5AU + VIDEO_FMT_COUNT))) {
          const video_fmt_t *fmt = &s_fmt[byte - 0x5AU];
          uint16_t src_rows = (uint16_t)(VIDEO_STRIP_ROWS / fmt->scale);
          s_fmt_index = (uint8_t)(byte - 0x5AU);
          s_strip_bytes = (uint32_t)fmt->src_w * src_rows * fmt->bpp;
          s_mask = 0U;
          s_mask_fill = 0U;
          s_video_state = VIDEO_READ_MASK;
          s_magic = 0U;
        } else {
          s_magic = (byte == VIDEO_MAGIC_0) ? 1U : 0U;
        }
        break;

      case VIDEO_READ_MASK:
        s_mask = (uint16_t)((s_mask >> 8) | ((uint16_t)byte << 8));
        ++s_mask_fill;
        if (s_mask_fill >= 2U) {
          s_data_strip = video_next_strip(0U);
          s_fill = 0U;
          if (s_data_strip >= VIDEO_STRIP_NUM) {
            s_video_state = VIDEO_WAIT_MAGIC; /* 无变化 */
            s_last_frame_tick = HAL_GetTick();
            ++g_video_frame_cnt;
          } else {
            s_video_state = VIDEO_READ_DATA;
          }
        }
        break;

      case VIDEO_READ_DATA:
        s_frame[(uint32_t)s_data_strip * s_strip_bytes + s_fill] = byte;
        ++s_fill;
        if (s_fill >= s_strip_bytes) {
          s_data_strip = video_next_strip((uint8_t)(s_data_strip + 1U));
          s_fill = 0U;
          if (s_data_strip >= VIDEO_STRIP_NUM) {
            video_present();
            s_video_state = VIDEO_WAIT_MAGIC;
            s_last_frame_tick = HAL_GetTick();
            ++g_video_frame_cnt;
          }
        }
        break;

      default:
        s_video_state = VIDEO_WAIT_MAGIC;
        break;
    }
  }
}
