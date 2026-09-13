#ifndef __USBD_CDC_IF_H
#define __USBD_CDC_IF_H

#include "usbd_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief CDC 端点定义。 */
#define CDC_IN_EP   0x81U
#define CDC_OUT_EP  0x01U
#define CDC_CMD_EP  0x82U

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H */
