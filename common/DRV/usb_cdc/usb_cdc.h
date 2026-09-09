/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_cdc.h
 * Description        : USB CDC ACM device interface.
 *******************************************************************************/

#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void USB_CDC_Init(void);
uint8_t USB_CDC_Write(const uint8_t *data, uint8_t length);
uint8_t USB_CDC_WriteBlocking(const uint8_t *data, uint8_t length);
int USB_CDC_PrintfWrite(const char *data, int length);
void USB_CDC_OnReceive(const uint8_t *data, uint8_t length);

#ifdef __cplusplus
}
#endif

#endif
