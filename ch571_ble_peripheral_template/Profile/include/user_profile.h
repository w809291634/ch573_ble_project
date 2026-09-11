/********************************** (C) COPYRIGHT *******************************
 * File Name          : user_profile.h
 * Description        : User Profile BLE 服务。
 *                      服务 UUID 0xFFF0，读写特性 0xFFF6（支持 READ/WRITE/
 *                      WRITE_WITHOUT_RSP/NOTIFY）。用于 BLE 调试助手特性收发。
 *******************************************************************************/

#ifndef USR_PROF_H
#define USR_PROF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CONFIG.h"

/*********************************************************************
 * 常量
 *********************************************************************/

/* User Profile 业务服务 / 特性 UUID（协议规定）。本工程仅用于特性收发调试。 */
#define USR_PROF_SERV_UUID    0xFFF0   /* 服务  0xFFF0 */
#define USR_PROF_CHAR_UUID    0xFFF6   /* 读写特性 0xFFF6 */

/* 特性值长度：承载一包裸数据。收到的主机写入会打印，读返回最近写入。 */
#define USR_PROF_CHAR_LEN     20

/* 接收回调：conn=连接句柄，pValue=收到数据，len=长度，method=0x12写请求/0x52无响应写 */
typedef void (*usr_prof_change_cb_t)(uint16_t connHandle, uint8_t *pValue, uint16_t len, uint8_t method);

typedef struct
{
    usr_prof_change_cb_t pfnUsrProfChange; /* 特性值变化（收到主机写入）回调 */
    void (*pfnUsrProfConnState)(uint16_t connHandle, uint8_t up); /* 连接状态回调：up=1 连接，0 断开 */
} usr_prof_cbs_t;

/*********************************************************************
 * API
 *********************************************************************/

bStatus_t UsrProf_AddService(uint32_t services);

bStatus_t UsrProf_RegisterAppCBs(usr_prof_cbs_t *appCallbacks);

/* 发送一包数据：打印发送内容，下发 FFF6 通知（需主机已订阅 CCCD）。 */
bStatus_t UsrProf_Send(uint16_t connHandle, uint8_t *pValue, uint16_t len);

/* 主机读取 FFF6 时返回的最近帧。 */
bStatus_t UsrProf_SetParameter(uint8_t len, void *value);

/* 当前连接句柄（供发送查用）；未连接返回 GAP_CONNHANDLE_INIT。 */
uint16_t UsrProf_GetConnHandle(void);

/* 连接状态入口：由 gapRole 连接/断开事件调用（见 peripheralStateNotificationCB）。
 * up=1 连接建立，0 连接断开。断开时清通知配置并复位连接句柄。 */
void UsrProf_OnConnState(uint16_t connHandle, uint8_t up);

#ifdef __cplusplus
}
#endif

#endif /* USR_PROF_H */
