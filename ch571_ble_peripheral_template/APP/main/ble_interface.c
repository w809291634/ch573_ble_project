/********************************** (C) COPYRIGHT *******************************
 * File Name          : ble_interface.c
 * Description        : User Profile BLE 数据收发统一入口。
 *                      接收：向 UsrProf 注册回调，集中处理主机下发数据。
 *                      发送：封装 FFF6 通知发送。
 *******************************************************************************/

#include "CONFIG.h"
#include "ble_interface.h"

#define DBG_TAG         "ble"
#define DBG_LVL         DBG_INFO
#include "debug_log.h"

#include "user_profile.h"

/*********************************************************************
 * 接收回调：UsrProf 收到主机写入时调用。
 * HEX 帧的 HEX 打印已在 profile 内完成，这里直接喂入协议层拼帧解析。
 *********************************************************************/
static void ble_rx_cb(uint16_t connHandle, uint8_t *pValue, uint16_t len, uint8_t method)
{
    (void)connHandle;
    (void)pValue;
    (void)len;
    (void)method;
}

/* 连接状态回调：将连接/断开状态通知应用层。
 * 同时记录当前连接句柄，供主动断开使用。 */
static uint16_t s_conn_handle = GAP_CONNHANDLE_INIT;

static void ble_conn_cb(uint16_t connHandle, uint8_t up)
{
    s_conn_handle = up ? connHandle : GAP_CONNHANDLE_INIT;
    log_i("conn_cb,0x%04X,%d", (unsigned)connHandle, up);
    // App_OnBleLinked(up);
}

/* UsrProf 回调结构（注册给 profile 层） */
static usr_prof_cbs_t s_usrProfCBs = {
    ble_rx_cb,
    ble_conn_cb
};

/*********************************************************************
 * 对外接口
 *********************************************************************/
void BleInterface_Init(void)
{
    UsrProf_RegisterAppCBs(&s_usrProfCBs);
    log_d("ble_interface init");
}

void BleInterface_Send(uint16_t connHandle, uint8_t *pData, uint16_t len)
{
    UsrProf_Send(connHandle, pData, len);
}

/* 主动断开当前激活的 BLE 连接。 */
void BleInterface_Disconnect(void)
{
    if(s_conn_handle != GAP_CONNHANDLE_INIT)
        GAPRole_TerminateLink(s_conn_handle);
}
