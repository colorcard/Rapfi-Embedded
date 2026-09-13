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
/** @brief 视频判定窗口。 */
#define VIDEO_ACTIVE_MS   300U
/** @brief 支持的帧格式数量。 */
#define VIDEO_FMT_COUNT   4U
/** @brief 整帧缓冲大小（字节）：按最大可缓冲帧（RGB332 全屏 67200）预留。 */
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

static uint8_t s_rx_fifo[USB_RX_FIFO_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

static uint8_t s_frame[VIDEO_FRAME_CAP];  /* 整帧缓冲（用于消撕裂） */
static uint8_t s_src[VIDEO_STRIP_PX * 2U];/* 渐进模式源条带 */
static uint8_t s_dst[VIDEO_STRIP_PX * 2U];/* 展开后的输出条带（RGB565 大端）*/
static uint32_t s_frame_bytes;            /* 本帧总字节数 */
static uint32_t s_frame_fill;             /* 已收字节数 */
static uint16_t s_src_target;             /* 渐进模式：单条带源字节数 */
static uint16_t s_src_fill;
static uint16_t s_frame_row;
static uint8_t s_video_state;
static uint8_t s_fmt_index;
static uint8_t s_buffered;                /* 1=整帧缓冲后一次刷屏 */
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

/**
 * @brief 将 RGB332 字节或 RGB565 大端字节转为 RGB565 值。
 */
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

/**
 * @brief 从整帧缓冲展开一个 16 行条带到 s_dst（不发送）。
 * @param frame 整帧缓冲。
 * @param out_row 目标起始行。
 * @return 无。
 */
static void video_expand_strip(const uint8_t *frame, uint16_t out_row)
{
  const video_fmt_t *fmt = &s_fmt[s_fmt_index];
  uint16_t oy;

  for (oy = 0U; oy < VIDEO_STRIP_ROWS; ++oy) {
    uint16_t oy_abs = (uint16_t)(out_row + oy);
    uint16_t sy = (uint16_t)(oy_abs / fmt->scale);
    uint16_t ox;
    for (ox = 0U; ox < LCD_WIDTH; ++ox) {
      uint16_t sx = (uint16_t)(ox / fmt->scale);
      uint32_t si = ((uint32_t)sy * fmt->src_w + sx) * (uint32_t)fmt->bpp;
      uint16_t col = video_pixel(&frame[si], fmt->bpp);
      uint32_t di = ((uint32_t)oy * LCD_WIDTH + ox) * 2U;
      s_dst[di] = (uint8_t)(col >> 8);
      s_dst[di + 1U] = (uint8_t)col;
    }
  }
}

/**
 * @brief 发送 s_dst 中的当前条带（片选由调用者保持）。
 * @return 无。
 */
static void video_tx_strip(void)
{
  (void)spi_write_8bit_array(SPI_1, s_dst, (uint16_t)(VIDEO_STRIP_PX * 2U),
                             1000U);
  ++g_video_strip_cnt;
}

/**
 * @brief 整帧收齐后：只设一次全屏窗口，连续刷完 15 条（最小化撕裂与间隙）。
 */
static void video_present_buffered(void)
{
  uint16_t row;

  if (lcd_hw_start_area(0U, 0U, LCD_WIDTH, LCD_HEIGHT) != 0) {
    return;
  }
  for (row = 0U; row < LCD_HEIGHT; row = (uint16_t)(row + VIDEO_STRIP_ROWS)) {
    video_expand_strip(s_frame, row);
    video_tx_strip();
  }
  lcd_hw_end_area();
}

/**
 * @brief 渐进模式：收满一个源条带即刷一条（内存不足以缓冲整帧时使用）。
 */
static void video_present_progressive(void)
{
  const video_fmt_t *fmt = &s_fmt[s_fmt_index];
  uint16_t src_rows = (uint16_t)(VIDEO_STRIP_ROWS / fmt->scale);
  uint16_t oy;

  for (oy = 0U; oy < VIDEO_STRIP_ROWS; ++oy) {
    uint16_t sy = (uint16_t)(oy / fmt->scale);
    uint16_t ox;
    (void)src_rows;
    for (ox = 0U; ox < LCD_WIDTH; ++ox) {
      uint16_t sx = (uint16_t)(ox / fmt->scale);
      uint32_t si = ((uint32_t)sy * fmt->src_w + sx) * (uint32_t)fmt->bpp;
      uint16_t col = video_pixel(&s_src[si], fmt->bpp);
      uint32_t di = ((uint32_t)oy * LCD_WIDTH + ox) * 2U;
      s_dst[di] = (uint8_t)(col >> 8);
      s_dst[di + 1U] = (uint8_t)col;
    }
  }
  if (lcd_hw_start_area(0U, s_frame_row, LCD_WIDTH, VIDEO_STRIP_ROWS) == 0) {
    video_tx_strip();
    lcd_hw_end_area();
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
      if (s_magic == 0U) {
        if (byte == VIDEO_MAGIC_0) {
          s_magic = 1U;
        }
      } else if ((byte >= 0x5AU) && (byte < (0x5AU + VIDEO_FMT_COUNT))) {
        const video_fmt_t *fmt = &s_fmt[byte - 0x5AU];
        uint16_t src_rows = (uint16_t)(LCD_HEIGHT / fmt->scale);
        s_fmt_index = (uint8_t)(byte - 0x5AU);
        s_frame_bytes = (uint32_t)fmt->src_w * src_rows * fmt->bpp;
        s_src_target = (uint16_t)(fmt->src_w * (VIDEO_STRIP_ROWS / fmt->scale)
                                  * fmt->bpp);
        s_buffered = (s_frame_bytes <= (uint32_t)sizeof(s_frame)) ? 1U : 0U;
        s_video_state = 1U;
        s_frame_row = 0U;
        s_frame_fill = 0U;
        s_src_fill = 0U;
        s_magic = 0U;
      } else {
        s_magic = (byte == VIDEO_MAGIC_0) ? 1U : 0U;
      }
      continue;
    }

    if (s_buffered != 0U) {
      if (s_frame_fill < (uint32_t)sizeof(s_frame)) {
        s_frame[s_frame_fill] = byte;
      }
      ++s_frame_fill;
      if (s_frame_fill >= s_frame_bytes) {
        video_present_buffered();
        s_video_state = 0U;
        s_last_frame_tick = HAL_GetTick();
        ++g_video_frame_cnt;
      }
    } else {
      s_src[s_src_fill] = byte;
      ++s_src_fill;
      if (s_src_fill >= s_src_target) {
        video_present_progressive();
        s_frame_row = (uint16_t)(s_frame_row + VIDEO_STRIP_ROWS);
        s_src_fill = 0U;
        if (s_frame_row >= LCD_HEIGHT) {
          s_video_state = 0U;
          s_last_frame_tick = HAL_GetTick();
          ++g_video_frame_cnt;
        }
      }
    }
  }
}
