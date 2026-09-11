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
#define DBG_LEVEL       DBG_LOG
#include "debug_log.h"

#include "user_profile.h"

/*********************************************************************
 * 数据打印：原始 log_draw 输出，格式 "<- / -> 长度: 逐字节 HEX"。
 * 方向 dir 传 "<- "（发送）或 "-> "（接收）。
 *********************************************************************/
/*********************************************************************
 * @fn      ble_dump
 * @brief   以 log_draw 输出一帧数据的方向、长度与逐字节内容。
 * @param   dir   - 方向标识："<- "=发送，"-> "=接收。
 * @param   pData - 数据缓冲。
 * @param   len   - 数据长度。
 * @return  none
 */
static void ble_dump(const char *dir, uint8_t *pData, uint16_t len)
{
    uint16_t i;

    log_draw("%s%u:", dir, (unsigned)len);
    for(i = 0; i < len; i++)
        log_draw(" %02X", pData[i]);
    log_draw("\r\n");
}

/*********************************************************************
 * 接收回调：UsrProf 收到主机写入时调用。
 * 打印接收方向数据，后续可接协议层拼帧解析。
 *********************************************************************/
/*********************************************************************
 * @fn      ble_rx_cb
 * @brief   接收 User Profile 主机写入回调。
 * @param   connHandle - BLE 连接句柄。
 * @param   pValue     - 收到的数据缓冲。
 * @param   len        - 收到的数据长度。
 * @param   method     - ATT 写入方式。
 * @return  none
 */
static void ble_rx_cb(uint16_t connHandle, uint8_t *pValue, uint16_t len, uint8_t method)
{
    (void)connHandle;
    (void)method;
    ble_dump("-> ", pValue, len);
}

/* 连接状态回调：将连接/断开状态通知应用层。
 * 同时记录当前连接句柄，供主动断开使用。 */
static uint16_t s_conn_handle = GAP_CONNHANDLE_INIT;

/*********************************************************************
 * @fn      ble_conn_cb
 * @brief   更新当前 BLE 连接句柄。
 * @param   connHandle - BLE 连接句柄。
 * @param   up         - 1-连接建立，0-连接断开。
 * @return  none
 */
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
/*********************************************************************
 * @fn      BleInterface_Init
 * @brief   注册 User Profile 收发回调。
 * @return  none
 */
void BleInterface_Init(void)
{
    UsrProf_RegisterAppCBs(&s_usrProfCBs);
    log_d("ble_interface init");
}

/*********************************************************************
 * @fn      BleInterface_Send
 * @brief   通过 User Profile 发送 BLE 通知数据。
 * @param   connHandle - BLE 连接句柄。
 * @param   pData      - 待发送数据缓冲。
 * @param   len        - 待发送数据长度。
 * @return  none
 */
void BleInterface_Send(uint16_t connHandle, uint8_t *pData, uint16_t len)
{
    ble_dump("<- ", pData, len);
    UsrProf_Send(connHandle, pData, len);
}

/*********************************************************************
 * @fn      BleInterface_Disconnect
 * @brief   主动断开当前激活的 BLE 连接。
 * @return  none
 */
void BleInterface_Disconnect(void)
{
    if(s_conn_handle != GAP_CONNHANDLE_INIT)
        GAPRole_TerminateLink(s_conn_handle);
}
