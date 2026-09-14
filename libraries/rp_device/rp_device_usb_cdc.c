#include "rp_device_usb_cdc.h"

#include <string.h>

#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_cdc_if.h"

/** @brief USB 设备句柄（供 CDC 接口层引用）。 */
USBD_HandleTypeDef hUsbDeviceFS;
/** @brief PCD 句柄（定义在 usbd_conf.c）。 */
extern PCD_HandleTypeDef hpcd_USB_FS;

/** @brief 接收环形缓冲大小（2 的幂，索引用掩码）。 */
#define USB_RX_FIFO_SIZE  4096U
#define USB_RX_FIFO_MASK  (USB_RX_FIFO_SIZE - 1U)
/** @brief 发送等待超时。 */
#define USB_TX_TIMEOUT_MS 200U

static uint8_t s_rx_fifo[USB_RX_FIFO_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

/** @brief CDC 接收回调（由 usbd_cdc_if.c 调用），把数据压入环形缓冲。 */
void rp_usb_cdc_on_rx(const uint8_t *data, uint32_t len)
{
  uint32_t head = s_rx_head;
  uint32_t i;

  if (data == NULL) {
    return;
  }
  for (i = 0U; i < len; ++i) {
    uint32_t next = (head + 1U) & USB_RX_FIFO_MASK;
    if (next == s_rx_tail) {
      break; /* 缓冲已满，丢弃剩余数据 */
    }
    s_rx_fifo[head] = data[i];
    head = next;
  }
  s_rx_head = head;
}

uint32_t usb_cdc_rx_available(void)
{
  return (s_rx_head - s_rx_tail) & USB_RX_FIFO_MASK;
}

uint32_t usb_cdc_read(uint8_t *data, uint32_t max_length)
{
  uint32_t avail;
  uint32_t count;
  uint32_t i;
  uint32_t tail = s_rx_tail;

  if (data == NULL) {
    return 0U;
  }
  avail = (s_rx_head - tail) & USB_RX_FIFO_MASK;
  count = (avail < max_length) ? avail : max_length;
  for (i = 0U; i < count; ++i) {
    data[i] = s_rx_fifo[tail];
    tail = (tail + 1U) & USB_RX_FIFO_MASK;
  }
  s_rx_tail = tail;
  return count;
}

rp_status_t usb_cdc_write(const uint8_t *data, uint32_t length)
{
  USBD_CDC_HandleTypeDef *hcdc =
      (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  uint32_t start;

  if ((data == NULL) || (length == 0U) || (length > 0xFFFFU)) {
    return RP_INVALID_PARAM;
  }
  start = HAL_GetTick();
  /* 等上一次发送完成，避免复用发送缓冲时被改写。 */
  while ((hcdc != NULL) && (hcdc->TxState != 0U)) {
    if ((HAL_GetTick() - start) > USB_TX_TIMEOUT_MS) {
      return RP_ERROR;
    }
  }
  if (CDC_Transmit_FS((uint8_t *)data, (uint16_t)length) != USBD_OK) {
    return RP_ERROR;
  }
  while ((hcdc != NULL) && (hcdc->TxState != 0U)) {
    if ((HAL_GetTick() - start) > USB_TX_TIMEOUT_MS) {
      return RP_ERROR;
    }
  }
  return RP_OK;
}

rp_status_t usb_cdc_init(void)
{
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK) {
    return RP_ERROR;
  }
  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK) {
    return RP_ERROR;
  }
  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) !=
      USBD_OK) {
    return RP_ERROR;
  }
  if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
    return RP_ERROR;
  }
  return RP_OK;
}

void usb_cdc_irq_handler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_FS);
}
