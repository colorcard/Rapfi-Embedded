#include "zf_device_usb_cdc.h"

#include <stdarg.h>
#include <stdio.h>

#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_cdc_if.h"

#include "zf_common_bsp_config.h"

/** @brief USB 设备句柄（供 CDC 接口层引用）。 */
USBD_HandleTypeDef hUsbDeviceFS;
/** @brief PCD 句柄（定义在 usbd_conf.c）。 */
extern PCD_HandleTypeDef hpcd_USB_FS;

/** @brief 接收环形缓冲大小。 */
#define USB_RX_FIFO_SIZE  4096U
/** @brief 发送等待超时。 */
#define USB_TX_TIMEOUT_MS 200U
/** @brief printf 组装缓冲大小。 */
#define USB_TX_BUF_SIZE   256U

static uint8_t s_rx_fifo[USB_RX_FIFO_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

/** @brief CDC 接收回调（由 usbd_cdc_if.c 调用），把数据压入环形缓冲。 */
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

uint32_t usb_cdc_read(uint8_t *data, uint32_t max_length)
{
  uint32_t count = 0U;

  if (data == NULL) {
    return 0U;
  }
  while ((count < max_length) && (usb_cdc_try_read_byte(&data[count]) != 0)) {
    ++count;
  }
  return count;
}

int usb_cdc_try_read_byte(uint8_t *out)
{
  if ((out == NULL) || (s_rx_tail == s_rx_head)) {
    return 0;
  }
  *out = s_rx_fifo[s_rx_tail];
  s_rx_tail = (s_rx_tail + 1U) % USB_RX_FIFO_SIZE;
  return 1;
}

zf_status_t usb_cdc_write(const uint8_t *data, uint32_t length)
{
  USBD_CDC_HandleTypeDef *hcdc =
      (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  uint32_t start;

  if ((data == NULL) || (length == 0U) || (length > 0xFFFFU)) {
    return ZF_INVALID_PARAM;
  }
  start = HAL_GetTick();
  /* 等上一次发送完成，避免复用发送缓冲时被改写。 */
  while ((hcdc != NULL) && (hcdc->TxState != 0U)) {
    if ((HAL_GetTick() - start) > USB_TX_TIMEOUT_MS) {
      return ZF_ERROR;
    }
  }
  if (CDC_Transmit_FS((uint8_t *)data, (uint16_t)length) != USBD_OK) {
    return ZF_ERROR;
  }
  /* 等本次发送真正完成，调用方即可复用缓冲。 */
  while ((hcdc != NULL) && (hcdc->TxState != 0U)) {
    if ((HAL_GetTick() - start) > USB_TX_TIMEOUT_MS) {
      return ZF_ERROR;
    }
  }
  return ZF_OK;
}

zf_status_t usb_cdc_write_str(const char *s)
{
  uint32_t n = 0U;

  if (s == NULL) {
    return ZF_INVALID_PARAM;
  }
  while (s[n] != '\0') {
    ++n;
  }
  return usb_cdc_write((const uint8_t *)s, n);
}

zf_status_t usb_cdc_printf(const char *fmt, ...)
{
  static uint8_t buf[USB_TX_BUF_SIZE];
  va_list args;
  int n;

  va_start(args, fmt);
  n = vsnprintf((char *)buf, sizeof(buf), fmt, args);
  va_end(args);
  if (n <= 0) {
    return ZF_ERROR;
  }
  if ((uint32_t)n >= sizeof(buf)) {
    n = (int)sizeof(buf) - 1;
  }
  return usb_cdc_write(buf, (uint32_t)n);
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
