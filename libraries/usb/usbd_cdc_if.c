#include "usbd_cdc_if.h"
#include "usbd_conf.h"

/** @brief CDC 收发缓冲大小。 */
#define APP_RX_DATA_SIZE  512U
#define APP_TX_DATA_SIZE  512U

extern USBD_HandleTypeDef hUsbDeviceFS;
/** @brief App 层收到的数据回调（定义在 zf_device_usb_cdc.c）。 */
extern void zf_usb_cdc_on_rx(const uint8_t *data, uint32_t len);

static uint8_t s_rx_buf[APP_RX_DATA_SIZE];
static uint8_t s_tx_buf[APP_TX_DATA_SIZE];

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitCplt_FS
};

static int8_t CDC_Init_FS(void)
{
  (void)USBD_CDC_SetTxBuffer(&hUsbDeviceFS, s_tx_buf, 0U);
  (void)USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_rx_buf);
  return (int8_t)USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
  return (int8_t)USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
  (void)length;
  switch (cmd) {
    case CDC_GET_LINE_CODING:
      /* 报告 115200 8N1（USB CDC 下波特率仅为信息）。 */
      pbuf[0] = 0x00U;
      pbuf[1] = 0xC2U;
      pbuf[2] = 0x01U;
      pbuf[3] = 0x00U;
      pbuf[4] = 0x00U;
      pbuf[5] = 0x00U;
      pbuf[6] = 0x08U;
      break;
    default:
      break;
  }
  return (int8_t)USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
  zf_usb_cdc_on_rx(Buf, *Len);
  (void)USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  (void)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (int8_t)USBD_OK;
}

static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
  (void)Buf;
  (void)Len;
  (void)epnum;
  return (int8_t)USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
  USBD_CDC_HandleTypeDef *hcdc =
      (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

  if (hcdc == NULL) {
    return USBD_FAIL;
  }
  if (hcdc->TxState != 0U) {
    return USBD_BUSY;
  }
  (void)USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
