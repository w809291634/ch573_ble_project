/********************************** (C) COPYRIGHT *******************************
 * File Name          : ble_interface.h
 * Description        : User Profile BLE 数据收发统一入口。
 *                      接收：向 UsrProf 注册回调，主机写入统一进入 BleInterface。
 *                      发送：封装 FFF6 通知发送，对外只调 BleInterface_Send。
 *                      业务协议解析与上报后续都集中在此模块维护。
 *******************************************************************************/

#ifndef BLE_INTERFACE_H
#define BLE_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*********************************************************************
 * API
 *********************************************************************/

/* 初始化：注册 UsrProf 接收回调。需在 GATT 服务添加后调用一次。 */
void BleInterface_Init(void);

/* 发送一包数据（经 FFF6 通知下发；connHandle 为当前连接句柄）。 */
void BleInterface_Send(uint16_t connHandle, uint8_t *pData, uint16_t len);

/* 主动断开当前激活（如有）的 BLE 连接。 */
void BleInterface_Disconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_INTERFACE_H */
